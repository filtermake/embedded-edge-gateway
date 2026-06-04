# M7 epoll Reactor + M9 timerfd — 面试速记卡

> 模块:epoll 事件循环 + Reactor/Channel 抽象 + timerfd 定时器 + 空闲连接超时踢出
> 覆盖:`src/net/EventLoop.{h,cpp}`、`src/net/timer_server.cpp`
> 组织原则:按面试官真实追问链 + 深挖频率排序(A > C > F > B/D/E),每张卡过两道筛子(真实会问 + 答它能提升表现)。
> 针对性:M7 模拟面 78 分暴露的老毛病——"不主动讲深度设计点、因果不精确"——本卡把该主动说的深度点固化为必答内容,并标注因果链。

---

## 主题 A:epoll 基础(必考门槛)

### A1. epoll 三个核心 API 各做什么?

- `epoll_create1(flags)`:创建 epoll 实例,返回 epfd(内核里的事件表)。用 `EPOLL_CLOEXEC` 防 fork 后子进程继承。
- `epoll_ctl(epfd, op, fd, event)`:增删改要监听的 fd(op = ADD/MOD/DEL)。
- `epoll_wait(epfd, events, max, timeout)`:阻塞等待,返回就绪的 fd 列表(只返回有事件的,不是全部)。

**深度点**:epoll 把"注册"(ctl)和"等待"(wait)分离了——fd 集合常驻内核,每次 wait 不用像 select 那样把整个集合从用户态拷进内核。这是 epoll 高效的根因之一。

### A2. epoll 比 select/poll 强在哪?为什么?

1. **无需每次重传 fd 集合**:select/poll 每次调用都把全部 fd 从用户态拷到内核;epoll 的 fd 常驻内核红黑树,ctl 时一次注册。
2. **返回就绪列表,不用遍历全部**:select 返回后要遍历所有 fd 挨个查谁就绪(O(n));epoll_wait 直接给就绪的那些(O(就绪数))。
3. **无 fd 数量上限**:select 受 FD_SETSIZE(通常 1024)限制;epoll 只受系统 fd 上限。

**因果要精确**:不要笼统说"epoll 快",要说"快在省了每次的集合拷贝 + 省了线性遍历"。连接数越多优势越大,这是为什么 epoll 适合 C10K。

**追问:select 还有用吗?**
连接数少(几十个)且要跨平台时,select 更简单可移植。epoll 是 Linux 专属。不是 epoll 全面碾压,是高并发场景 epoll 胜。

### A3. ET 和 LT 的区别?为什么 ET 必须配非阻塞 fd?

- **LT(水平触发,默认)**:只要 fd 可读/可写就持续通知。读一次没读完,下次 wait 还会报。
- **ET(边沿触发)**:只在状态变化时通知一次。没读完不会再报,必须一次读干净。
- **ET 为什么配非阻塞**:ET 下必须循环 read 到读空。若是阻塞 fd,读空后再 read 会阻塞住整个线程(Reactor 单线程,一阻塞全卡死);非阻塞 fd 读空时返回 EAGAIN,据此 break。

**实践结合**:我在 HTTP server 里就是 ET + 非阻塞,on_read 循环 recv 到 EAGAIN。

### A4. epoll_wait 返回后怎么知道是哪个 fd、什么事件?data.ptr 存了什么?

`epoll_event` 有个 `data` 联合体,可存 fd 或 ptr。我的 Reactor 里 `ev.data.ptr` 存 Channel 的裸指针(`ch.get()`)。epoll_wait 返回的每个 event 里,`events[i].data.ptr` 强转回 `channel*` 就拿到该 fd 对应的 Channel,进而调它的回调。`events[i].events` 是实际就绪的事件位(EPOLLIN/EPOLLOUT)。

这是 Reactor 把"裸 fd 事件"升级成"对象回调"的关键一跳。

---

## 主题 C:Channel 生命周期与 UAF 防护(高光块)

### C1. Channel 为什么用 shared_ptr 托管?map 存 shared_ptr、epoll 存裸指针,不会出问题吗?

- **map 存 `shared_ptr<channel>`**:所有权持有者,Channel 生命周期由它决定,在 map 里就活着。
- **epoll 存裸指针 `ch.get()`**:只是借用,epoll 不管死活,只在事件就绪时拿它回调。
- **不冲突的前提**:裸指针的有效期被 shared_ptr 的有效期覆盖——只要 Channel 还在 map 里,epoll 拿到的裸指针就有效。

**因果**:为什么 epoll 里不也存 shared_ptr?因为 `epoll_event.data` 是 union,只能存裸指针/fd/整数,存不下带引用计数的 shared_ptr。所以必然是"shared_ptr 管生命周期 + 裸指针给 epoll 用"的组合——不是设计偏好,是 epoll API 约束逼出来的。

### C2. Channel 从 map 删除(shared_ptr 析构)但 epoll 裸指针还在,这不是 UAF 吗?怎么防的?

**危险场景**:一次 epoll_wait 返回一批就绪 events。处理第 1 个 event 时,回调里 `removeChannel` 删掉第 5 个 fd 的 Channel(超时踢出、对端关闭等)。若此时 shared_ptr 立即析构、Channel 被 delete,轮到处理第 5 个 event 时,`data.ptr` 指向已释放内存 → UAF。

**防护:延迟销毁 dying_**。removeChannel 时不立即销毁,把 Channel 的 shared_ptr 从 map 移到 `dying_` 向量续命,撑过当前这一批事件处理。等整批 events 处理完(for 循环结束)才 `dying_.clear()` 统一销毁。Channel 实际析构被推迟到"本轮所有事件处理完"之后,这一批里任何残留裸指针访问都还指向有效内存。

**一句话因果链**:epoll 一次返回一批事件 → 处理某个事件时可能删掉同批的另一个 Channel → 立即销毁会让后续事件的裸指针变野 → 用 dying_ 把销毁推迟到批末,保证整批处理期间所有 Channel 都活着。

**追问:为什么 clear 放 for 循环之后而非里面?**
放里面就等于立即销毁,失去意义。必须在整批 events 遍历完之后,这是延迟的"延迟点"所在。

### C3. removeChannel 的完整动作?顺序能换吗?

1. `epoll_ctl(DEL)`:先从 epoll 注销,之后不再产生这个 fd 的新事件。
2. 从 `channels_` map 找到并 erase——erase 前先把 shared_ptr `push_back` 进 `dying_` 续命。
3. fd 的实际 close 由 Channel 析构 RAII 完成(`if(fd!=-1) close(fd)`),不在 removeChannel 里手动 close。

**顺序要点**:先 epoll DEL 再动 map。若先删 map(Channel 可能析构、fd 被 close),再 epoll DEL 一个已关闭 fd,轻则 EBADF 警告,重则那个 fd 号被新连接复用后误删别人的注册。先 DEL 断掉事件源,再处理对象生命周期,安全。

### C4. fd 的 close 为什么交给 Channel 析构(RAII),不在 removeChannel 手动 close?

Channel 持有 fd 这个独占资源,析构里 `close(fd)` 就是标准 RAII——资源释放和对象生命周期绑定。好处:
- 不管 Channel 通过哪条路径销毁(正常删除、异常、dying_ 批量清理),fd 一定被关,不泄漏。
- 配合 `=delete` 拷贝,防两个 Channel 持有同一 fd 导致 double close。
- 手动 close 容易漏(异常路径、提前 return),RAII 让它无法漏。

连到 M4 SerialPort 的 RAII:同一原则(fd 跟对象同生共死)在网络模块的复用。

---

## 主题 F:空闲连接超时踢出(你的真坑块)

### F1. 怎么实现空闲连接超时踢出?数据结构和扫描逻辑?

- Channel 加 `time_t last_active`:每次该连接有读事件就更新为当前时间。
- timerfd 周期触发(每秒一次),回调里遍历所有 Channel,`now - last_active > TIMEOUT` 的就是空闲超时,踢掉。
- Channel 加 `bool timeout_exempt`:标记不参与超时扫描的 fd(listen_fd、timerfd 自己),扫描时跳过。

### F2. 超时扫描为什么要"两阶段"——先收集 fd 再统一删除,不能边遍历边删吗?

**核心坑**:边遍历 `channels_` map 边 `removeChannel`(它内部 `channels_.erase`)会**迭代器失效**——正在遍历的容器被删元素,迭代器悬空,UB。

**两阶段做法**:
1. 阶段一:遍历 `channels_`,把超时的 fd **收集**进一个 `vector<int> timeout_fds`(只读,不动 map)。
2. 阶段二:遍历 `timeout_fds`,对每个 fd 调 `removeChannel`(这时才动 map)。

遍历和修改分离,避免迭代器失效。

**因果链**:边遍历边删 → 迭代器失效 UB → 拆成"先只读收集 fd / 再统一删除",遍历期间不碰被遍历的容器。

**追问:这和 dying_ 延迟销毁是一回事吗?**
不是,是两层不同的保护。两阶段扫描防的是**遍历 channels_ map 时的迭代器失效**(STL 容器层面);dying_ 防的是**同批 epoll 事件处理时裸指针变野**(epoll 事件层面)。removeChannel 同时受益于两者:被两阶段扫描安全地调用,内部又用 dying_ 安全地延迟销毁。

### F3. 你踩过的 listen_fd 被超时误杀 bug 是怎么回事?怎么发现、怎么修?

**bug**:listen_fd(监听套接字)的 `last_active` 一直是初始值(它从不"收数据",只 accept),超时扫描时 `now - last_active` 必然大于 TIMEOUT,于是 listen_fd 被当成空闲连接踢掉——之后服务器再也无法接受新连接。

**发现**:压测时让一批空闲连接被踢,踢完后新连接连不上,定位到 listen_fd 被误删。

**修复**:给 Channel 加 `timeout_exempt` 标志,listen_fd 和 timerfd 都设为 true,扫描时 `if (ch->timeout_exempt) return` 跳过。

**价值**:这是"通用机制套到特殊 fd 上暴露的边界问题"——超时逻辑本为客户端连接设计,但 map 里还混着 listen_fd/timerfd 这些"非连接 fd"。能主动讲"我意识到不是所有 fd 都该参与超时,加了豁免标志"体现边界思维。配套压测脚本专门验证了"踢人期间 listen_fd 健在、新连接仍能 accept"。

---

## 主题 B:Reactor 模式 + Channel 抽象(带过)

### B1. 什么是 Reactor 模式?Channel 起什么作用?

Reactor = "事件多路分发器":单线程 epoll_wait 等待多个 fd 的事件,事件就绪时**分发**(dispatch)给对应的处理器(回调)。它把"等事件"和"处理事件"解耦,一个线程管成千上万连接。

**Channel** 是对"一个 fd + 它关心的事件 + 它的回调"的封装:`fd`、`events`(EPOLLIN/OUT)、`on_read`/`on_write` 回调。它让 EventLoop 不用关心"这个 fd 是 listen 还是 client 还是 timer",统一当 Channel 处理,事件来了调对应回调即可。listen/client/timer 都是 Channel,只是回调不同——这是抽象的威力。

---

## 主题 D:EPOLLOUT 异步写(带过)

### D1. 为什么要 out_buf 写缓冲?EPOLLOUT 何时开、何时关?

send 可能因内核发送缓冲满返回 EAGAIN(没发完)。剩余数据存进 Channel 的 `out_buf`,并注册 EPOLLOUT;等 fd 可写时(EPOLLOUT 触发)在 on_write 里继续发 out_buf。发完后**关掉 EPOLLOUT**(`events &= ~EPOLLOUT` + modifyChannel)——因为 fd 大部分时间可写,若不关,epoll 会持续报 EPOLLOUT 导致 busy loop 空转。

**关键**:EPOLLOUT 是"按需开启"的——只在有数据没发完时开,发完立即关。这叫 "write-on-demand"。

**追问:sendData 为什么抽成独立函数而非写进 lambda?**
避免 lambda 循环引用 / 捕获复杂度。sendData 接收 `(loop, channel*, data, len)` 显式参数,逻辑清晰,on_read 和 on_write 都能复用同一套发送 + EAGAIN 缓冲逻辑。

---

## 主题 E:timerfd 机制(带过)

### E1. 为什么用 timerfd 而非 signal/SIGALRM 做定时?

timerfd 把"定时"变成一个**可读的 fd**:定时到期时 fd 变可读,read 出一个 uint64 表示期间到期次数。好处:它能像普通 fd 一样**纳入 epoll 统一管理**,和 socket 事件在同一个 epoll_wait 里处理,不用信号那套(信号异步打断、不可重入、和主循环难协调)。

**零改框架**:我把 timerfd 封成一个普通 Channel(fd=timerfd, events=EPOLLIN, on_read=扫描超时),EventLoop 完全不用为定时器加特殊逻辑——这正是 Channel 抽象的价值:任何"可被 epoll 监听的 fd"都能套进 Channel。

**细节**:on_read 里必须 `read(timerfd, &exp, 8)` 读掉那 8 字节,否则 LT 模式下会反复触发。exp 是期间到期次数(慢回调时可能 >1)。

---

*作者:Bi(2026 秋招准备项目)*
*配套实测:echo_server / timer_server 编译运行通过;8 连接同轮超时踢出压测全绿(批量踢出正常 + 活跃连接不误杀 + listen_fd 健在)*
