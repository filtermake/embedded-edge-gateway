# M12 SQLite 面试 Flashcard

> 适用范围:嵌入式边缘网关本地持久化。SQLite C API(`sqlite3.h`)。
> 关联模块:M4(RAII 同构)、M7/M9(并发写入场景)、M3(异步落盘思路对照)。

---

## 卡 1 — SQLite 是什么?为什么边缘网关选它?

SQLite 不是独立 server 进程,而是**链接进进程的库**:程序直接读写一个 `.db` 文件,用文件锁做并发控制。
选它的理由:零配置、零运维、单文件、嵌入式无依赖。对单机、轻量、要持久化的边缘网关是最优解。
对照:MySQL/PostgreSQL 是 C/S 架构,要起独立服务进程,网关上是过度设计。

---

## 卡 2 — exec vs prepared statement,什么时候用哪个?

- `sqlite3_exec`:一次性执行完整 SQL 字符串,**只适合无外部参数**的语句——建表、建索引、PRAGMA。
- prepared statement(`prepare/bind/step/finalize`):**一切带外部数据**的 INSERT/SELECT 必用。
- 两个理由:
  1. **防 SQL 注入** —— 值走 bind,不拼进 SQL 文本,结构与数据天然隔离。
  2. **编译复用** —— SQL 只解析编译一次,同一语句可 reset 后反复执行。

---

## 卡 3 — prepared statement 四件套

1. `sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr)` —— 编译 SQL 成字节码,生成 stmt 句柄。`-1` = 自动算字符串长度;用 `_v2`(老版本有坑)。
2. `sqlite3_bind_*(stmt, idx, val, ...)` —— 把值填进 `?` 占位符。
3. `sqlite3_step(stmt)` —— 执行。INSERT 返回 `SQLITE_DONE`;SELECT 每行返回 `SQLITE_ROW`,取完返回 `SQLITE_DONE`。
4. `sqlite3_finalize(stmt)` —— 销毁句柄,不调则泄漏。

---

## 卡 4 — 【高频坑】bind 和 column 的索引方向相反

- `sqlite3_bind_*` 占位符索引**从 1 开始**(对应 SQL 习惯,第 1 个 `?` 是 1)。
- `sqlite3_column_*` 列索引**从 0 开始**(对应 C 数组下标)。
- 几乎人人踩过一次。记法:bind=SQL 编号(从1),column=数组下标(从0)。

---

## 卡 5 — bind_text 的 SQLITE_TRANSIENT vs SQLITE_STATIC

`sqlite3_bind_text(stmt, i, ptr, -1, destructor)` 最后一个参数控制内存所有权:
- `SQLITE_TRANSIENT`:告诉 SQLite「这块内存我马上会释放/改写,你自己复制一份」。**默认安全选项**。
- `SQLITE_STATIC`:「这块内存在 step 期间保证不变,别复制」。仅当能确保生命周期时用。
- 用错 STATIC 配栈上临时字符串 → 悬垂读 → UAF。不确定就 TRANSIENT。

---

## 卡 6 — 【高频坑】column 取出的内存生命周期

`sqlite3_column_text/blob` 返回的指针**只在下次 `step` 或 `finalize` 之前有效**。
要长期保存必须**当场拷贝**(构造成 `std::string`),不能存裸指针,否则悬垂读。
封装层应在接口边界处直接拷成 `std::string` 返回,把坑堵死。
附:`column_text` 返回 `const unsigned char*`,要 `reinterpret_cast<const char*>` 才能喂给 string/cout。

---

## 卡 7 — CREATE TABLE 与 IF NOT EXISTS 的语义

- `CREATE TABLE`(无 IF NOT EXISTS)+ 表已存在 → **报错**,数据不动,新表没建成。
- `CREATE TABLE IF NOT EXISTS` + 表已存在 → **静默跳过**,数据不动。
- `DROP TABLE` + `CREATE` → 这才是「重建」,**数据全没**。
- 关键:CREATE TABLE 永不覆盖已有表。网关每次启动跑建表 SQL,靠 IF NOT EXISTS 保住数据。
- 误区纠正:不写 IF NOT EXISTS 不会「重建覆盖」,而是报错。

---

## 卡 8 — RAII 封装 Statement(与 M4 SerialPort 同构)

- 构造即 `prepare`,失败抛异常(不留半成品对象)。
- 析构即 `finalize`,`noexcept`。
- `=delete` 拷贝(句柄独占,拷了会 double finalize)。
- 移动构造 + 移动赋值:转移后源指针置空,移动赋值带自赋值保护 + 先释放原持有。
- 价值:多出口函数 / 异常路径也保证 finalize,手动配对释放靠不住。

---

## 卡 9 — 所有权边界:连接 vs 语句

一个连接(`sqlite3*`)上可开多条语句(`sqlite3_stmt*`),连接活得比语句长。
所以 `Statement` 类**拥有 stmt(负责 finalize),不拥有 db(只借用)**,绝不 close db。
所有权边界划清是 review 必问点。

---

## 卡 10 — 【性能】事务:批量写为什么快两个数量级

- 默认每条 INSERT 是独立事务,执行后 fsync 刷盘 → N 条 = N 次 fsync(磁盘物理写,慢)。
- 包进一个显式事务:`BEGIN TRANSACTION; ... COMMIT;` → 只在 COMMIT 时刷一次盘。
- 1 万条插入实测可从几十秒降到几百毫秒(N 次 fsync → 1 次)。
- 网关攒一批传感器数据再批量落库,必走事务。

---

## 卡 11 — sqlite3_reset:热语句复用

`sqlite3_reset(stmt)` 把已 step 过的语句**倒回初始可执行态**,以便换参数再跑(≠ finalize 销毁)。
完整性能形态:**prepare 一次 → (reset + 重 bind + step) 多次**。
事务循环里复用同一个 prepared stmt 就靠 reset。

---

## 卡 12 — 【性能/并发】PRAGMA journal_mode = WAL

- 默认 rollback journal 模式:读写互斥(写时读被挡)。
- WAL(Write-Ahead Logging):写追加到 WAL 文件,读仍可读主库 → **读写并发**。
- 网关「网络线程写 + 查询线程读」并发场景几乎必开 WAL。
- 面试问「SQLite 怎么提升并发」→ 答 WAL。
- 配套:`PRAGMA synchronous = NORMAL`(WAL 下从 FULL 降一级,牺牲极端断电安全换吞吐,边缘设备常用)。

---

## 一句话总纲(面试可直接背)

> exec 管无参 DDL,prepared 管带数据 DML(防注入 + 编译复用);
> 连接 > 语句,Statement 拥有 stmt 不拥有 db,column 内存当场拷贝,RAII 兜异常;
> 性能三件套:批量写包事务、热语句 reset 复用、并发开 WAL。
