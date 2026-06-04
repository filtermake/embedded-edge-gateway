# M11 HTTP 解析器 — 面试速记卡

> 模块:Buffer + HTTP 四状态 FSM + 接入 epoll Reactor
> 覆盖:`src/net/Buffer.h`、`src/net/HttpRequest.{h,cpp}`、`src/net/http_server.cpp`
> 组织原则:按面试官真实追问链 + 深挖频率排序(D > B > C > A > E),每张卡过两道筛子(真实会问 + 答它能提升表现)。

---

## 主题 D:接入 epoll Reactor(频率最高)

### D1. ET 模式下,on_read 为什么必须 `while` 循环 recv 到 EAGAIN?只 recv 一次会怎样?

ET(边沿触发)只在 fd 状态**发生变化**时通知一次。一次唤醒后若只 recv 一次没读干净,内核缓冲区剩余数据**不会再触发新的 EPOLLIN**,会卡在内核里,直到下次有新数据到达才再触发。表现:处理延迟,甚至对端不再发数据时永久卡住。所以 ET 必须循环 recv 直到返回 EAGAIN,确认内核缓冲读空。

**追问:LT 需要这样吗?**
不需要。LT 只要缓冲区还有数据就持续通知,读一次也行,下次 epoll_wait 还会再报。但 LT 在数据没读完时会反复唤醒、空转。ET 更高效但要求一次读干净,代价是代码复杂度。选 ET 要能说出这个权衡。

### D2. parse 应该每次 recv 后调,还是读到 EAGAIN 攒齐再调一次?

选**每次 recv 后就 parse**(muduo 做法)。理由:符合 Reactor "数据可用就尽快处理" 的哲学,降低延迟。攒齐再 parse 也能工作,但响应延迟更高。两者都正确,关键是说出权衡——这题考"你知不知道有这个选择"。

**精确措辞**:不是"每次 recv 调一次 parse 就够",而是"每次 recv 后进入 parse 的内层循环,把 buffer 里当前所有完整请求都解析掉"。

### D3. 为什么 kComplete 之后要再套一层 while 继续 parse?

**pipelining** —— 一次 recv 可能带来多条完整请求(HTTP/1.1 允许连发)。解析出一条、发响应、reset() 后,buffer 里可能还有下一条,必须继续 parse 直到 kIncomplete。若不套这层 while,只处理一条就 break,剩余请求要等下次 recv——但 ET 下若对端不再发数据,这次 recv 已把数据全搬进 buffer,**不会再有 EPOLLIN 触发**,剩余请求永久饿死。

**亮点(ET + pipelining 叠加坑)**:ET 要求读干净(数据进 buffer),pipelining 要求解析干净(buffer 里请求全处理)。两个"干净"缺一不可。讲出这个交互很加分。

### D4. g_conns(fd→连接上下文)这张表,erase 时机?erase 后还能碰 conn 吗?

连接关闭时(recv 返回 0 / kError / recv 出错)`removeChannel` + `g_conns.erase(fd)`。**erase 之后 `conn` 引用立即悬空,绝对不能再访问**——每个 erase 后必须紧跟 return。`HttpConn& conn = g_conns[fd]` 是引用,map 删元素后引用失效,再用是 UAF。

**追问:漏 erase 会怎样?**
fd 会复用。连接 A 用 fd=7 关闭后没 erase,新连接 B 又分到 fd=7,`g_conns[7]` 拿到 A 的残留 buffer 和半解析状态,B 的请求就乱了。静默 bug,不崩,行为错。

**追问:channel 销毁和 g_conns.erase 冲突吗?**
不冲突,两张表。channel 由 EventLoop 的 dying_ 延迟到批末销毁(防 UAF);g_conns 是外挂的另一张表。on_read 执行期间 ch_raw 还活着(dying_ 兜底),return 后才真正析构。

---

## 主题 B:HTTP 请求解析状态机

### B1. 你的 HTTP FSM 和 M5 协议 FSM 核心区别是什么?

**驱动粒度不同**。M5 面向字节——二进制帧,逐字节 feed(),每字节驱动一次状态转移。HTTP 面向行——请求行、每条 header 都以 `\r\n` 结尾,解析单位是"一整行":用 `std::search` 切出 `\r\n` 之间的完整行,整行处理后再切下一行。

**追问:为什么 HTTP 不逐字节?**
可以,但面向行更自然:HTTP 的语法单元就是行,逐字节还得自己攒到 `\r\n` 才能处理,把切行逻辑摊进状态机更乱。面向行让一个状态处理一种行。

**追问:那 body 呢?**
body 是例外,不面向行,是 Content-Length 指定长度的纯字节,所以 ExpectBody 不找 `\r\n`,只数够字节数。**HTTP 是混合协议:header 面向行,body 面向长度。**

### B2. 四个状态怎么划分?转移条件?

`ExpectRequestLine → ExpectHeaders → ExpectBody → GotAll`
- RequestLine→Headers:切出请求行,解析 method/path/version 成功。
- Headers→Body 或 →GotAll:读到空行(只有 `\r\n` 的行,header 结束标志),按 Content-Length:>0 去 Body,否则直接 GotAll。
- Body→GotAll:收够 Content-Length 字节。

**追问:怎么判断"空行"而非普通 header?**
切出的行里找冒号 `:`,找不到(`colon == crlf`,行长度 0)就是空行。普通 header 必有冒号分隔 key/value。

### B3. parse 里为什么用 `while(hasMore)` 大循环包着所有状态?

一次 recv 的数据可能同时含完整请求行 + 完整 headers + 部分 body。若一次调用只处理一个状态就返回,请求行解析完转 ExpectHeaders 就退出了,headers 明明在 buffer 里却要等下次。while 让状态在单次调用内连续推进,把 buffer 里能解析的一次性榨干,直到 GotAll 或半包退出。

**一句话**:while 让一次 parse 跨越多个状态连续推进,把当前 buffer 里所有能解析的都解析掉。别说成"为了处理多条请求"——那是 pipelining,附带效果,不是主目的。

### B4. 半包(请求分多次 recv 到达)怎么处理?数据怎么拼起来?

核心在 **Buffer 留住未读数据 + parse 的状态保持**:
- 某状态找不到完整 `\r\n`(std::search 返回末尾),说明这行没收全,parse 返回 kIncomplete,**state_ 保持不动**。
- 已解析部分通过 retrieve 消费(readerIndex_ 推进),未解析的留在 buffer。
- 下次 recv 的新数据 append 进来时,Buffer 的 makeSpace 把未读旧数据和新数据**腾挪到连续内存**,于是 `Ho` + `st:` 拼成 `Host:`,std::search 一找就找到。
- parse 重新调用时 state_ 还停在上次状态,接着往下解析。

**关键**:parse 本身完全不关心数据分几次来。半包拼接由 Buffer 负责(留数据 + 腾挪),状态续传由 state_ 负责(不重置)。两者解耦。

---

## 主题 C:设计权衡(亮点卡)

### C1. HttpRequest 既存状态又存结果,muduo 拆成 HttpContext + HttpRequest,你为什么没拆?代价是什么?

- **代价**:HttpRequest 走到 GotAll 后,state_ 卡终态、字段是旧值,长连接复用必须 reset() 清空才能解析下一条。状态(解析器的事)和结果(数据的事)耦合,职责不单一。
- **muduo 拆法**:HttpContext 持有状态机 + parse,HttpRequest 退化成纯数据容器,用完即取走,边界干净。
- **我的选择**:第一版加 reset() 凑合,当前复用需求没复杂到必须拆。**YAGNI——不为没到来的复杂度过度设计。** 真要更复杂的连接状态管理时再拆。

**叙事价值**:"我评估过拆分,理解边界和代价,选了更简单的方案并说得出何时该改。" 反向亮点——知道为什么可以**不**那么分。何时不过度设计,和会设计一样重要。

### C2. reset() 漏调会怎样?崩溃还是别的?

**不崩,静默错误**。不 reset,state_ 停 GotAll,下次 parse 进 while 直接进 GotAll 分支立即退出,实质没解析,返回 kComplete 但 path() 等字段是上一条旧值。比崩溃更隐蔽——不报错、行为错。靠测试覆盖(Test 7 复用测试专门测这个)。能主动说"我写了复用测试防它"是加分。

---

## 主题 A:Buffer(精选)

### A1. 接收缓冲为什么不用裸 `char buf[]`,要专门设计 Buffer 类?

裸数组干不了两件事:
1. **retrieve 消费已解析数据**:解析完请求行要"删掉"这部分,裸数组删前 n 字节要 memmove 整体前移 O(n);Buffer 用 readerIndex_ 推进 O(1)。
2. **跨多次 recv 累积**:半包时旧数据要留着等新数据拼接,裸数组每次 recv 从头覆盖留不住。

Buffer 用双游标 + 底层 vector 解决两者。

### A2. makeSpace 为什么分两种情况?何时腾挪、何时真扩容?

写入空间不足时:
- **情况 A(腾挪)**:`readerIndex_ + writableBytes() >= len`——已读废弃区 + 尾部空闲够用,把未读数据整体挪到 buffer 最前面腾出尾部,**不扩容**,复用已读区避免无限增长。
- **情况 B(扩容)**:加起来还不够,才 resize。

**追问:腾挪时为什么先存 readable 再挪?**
挪完要重设游标,新 writerIndex_ 依赖"原来有多少可读数据"。若先把 readerIndex_ 改 0 再算 readableBytes() 就错了。典型的"先保存依赖量再改状态"。

**措辞陷阱**:游标归零是"空间复用",**不是"内存释放"**——capacity 一字节没变。说"释放内存"会被抓。

---

## 主题 E:M9 + M11 结合(点睛)

### E1. 空闲超时踢出(M9)接到 HTTP 上,对应真实 web server 的什么机制?

**keep-alive 超时管理**。HTTP/1.1 默认长连接,复用省去重复握手开销;但空闲连接不能永远占着 fd,所以生产 web server 都有 keep-alive timeout(nginx 默认 75s,我设 5s)。M9 的 timerfd + last_active + 超时扫描踢出,接到 HTTP 上正好就是这个机制。

**价值**:把 M9 定时器、M11 HTTP 两个独立里程碑串成有真实工程意义的整体。讲"我的超时机制对应 nginx 的 keepalive_timeout"显出理解的是真实系统,不是玩具。

---

*作者:Bi(2026 秋招准备项目)*
*配套实测:7 条单测全绿 + curl 端到端(GET/POST/keep-alive 连发/超时踢出)全通过*
