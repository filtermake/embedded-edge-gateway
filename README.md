# embedded-edge-gateway

C++17 多协议嵌入式 Linux 边缘网关,目标运行环境:Raspberry Pi 4B。

完整数据流:串口采集(M4)→ 帧协议解析(M5)→ 线程池(M8)→ 双写【SQLite 落库(M12) + MQTT 上行发布(M10)】,并内嵌 HTTP 监控服务(M7/M9/M11)提供实时网页 + uPlot 历史曲线。

## 模块状态

| 模块 | 路径 | 状态 |
|---|---|---|
| M3 日志 | `src/log/` | ✅ Logger 同步外壳 + AsyncLogger 双缓冲后端 / Meyers 单例 / 后台 flush 线程(3 秒间隔)/ 落盘 `/tmp/gateway.log` |
| M4 串口 | `src/serial/` | ✅ termios RAII 类 / Rule of Five / 8N1 / VMIN=1 VTIME=0 |
| M5 协议 | `src/protocol/` | ✅ 8 状态 FSM + CRC16-MODBUS,详见 [docs/m5_frame_protocol.md](docs/m5_frame_protocol.md) |
| M6 队列 | `src/concurrent/ThreadSafeQueue.h` | ✅ 模板类 / mutex + 双 cv / 有界容量 / shutdown 排空语义 |
| M8 线程池 | `src/concurrent/ThreadPool.h` | ✅ 基于 M6 队列 / `submit()` 返回 `std::future` / RAII join 工作线程 |
| M7 Reactor | `src/net/EventLoop.*` | ✅ epoll(ET)事件循环 + channel RAII(禁拷贝 / move)/ 读写回调 |
| M9 定时器 | `src/net/HttpServer.cpp` | ✅ timerfd 周期定时 + 空闲连接超时踢出(集成进 HTTP 服务) |
| M11 HTTP | `src/net/HttpRequest.h` | ✅ Buffer 双游标 + HTTP 四状态 FSM(请求行 / 头 / body)/ keep-alive 超时,curl 端到端 + 单测 7 条 |
| Web 监控 | `src/net/HttpServer.*` + `web/` | ✅ 内嵌 HTTP 服务:`/api/data` 查只读 SQLite 返回 JSON + 内嵌监控页 + uPlot 历史曲线;web 资源编译期 asset embedding 进二进制(离线自包含) |
| M12 SQLite | `src/db/` | ✅ Statement RAII + Database(连接锁 + 缓存 prepared insert + WAL);只读连接模式 + `query()` + 复合索引 `idx_dev_ts` |
| M10 MQTT | `src/mqtt/` | ✅ MqttClient RAII 封装 libmosquitto;`publish` 加锁线程安全;上行发布 + 下行订阅 |
| M14 构建 | `CMakeLists.txt` | ✅ CMake 3.15+ / C++17 / Modern CMake(INTERFACE 库收纳选项) |

## Build

依赖(M12 SQLite / M10 MQTT 需要):
```bash
sudo apt install -y libsqlite3-dev libmosquitto-dev
```

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

## 运行完整网关(项目一端到端)

`gateway` 单进程串起完整边缘网关数据流:

```
串口(M4) → 帧FSM解析(M5) → decodeFrame业务解码 → 线程池(M8)
                                                   ├─ SQLite 落库(M12)   /tmp/gateway.db
                                                   └─ MQTT 上行发布(M10) gateway/up/<dev>

HTTP 监控线程(独立只读连接,WAL 读写并发) ← 浏览器 http://localhost:8888
下行命令通道:订阅 gateway/cmd/#(占位回调,具体命令待 P2 联调)
```

关键设计:
- **双写**:每条记录由线程池 worker 同时落本地 SQLite 和 MQTT 上行发布,互不阻塞。
- **读写并发**:HTTP 查询用独立的【只读】SQLite 连接(`SQLITE_OPEN_READONLY`),靠 WAL 与主链写连接并发,查询不阻塞落库。
- **RAII 析构顺序**:`pool` 最后声明 => 最先析构(drain 在飞任务),此时 `db`/`client` 仍存活,无 UAF。
- **致命退出**:broker 连不上 / 串口打不开 / 磁盘异常统一被 catch,优雅致命退出。

### 启动步骤(需 4 个终端)

依赖 MQTT broker(网关启动即连接,连不上会退出):
```bash
sudo systemctl start mosquitto          # 或 mosquitto -c /etc/mosquitto/mosquitto.conf
```

终端 1 — 虚拟串口对(创建 `/tmp/ttyV0` 网关侧、`/tmp/ttyV1` STM32 侧),保持运行:
```bash
./scripts/start_vserial.sh
```

终端 2 — 启动网关:
```bash
./build/gateway /tmp/ttyV0
```

终端 3 — 假 STM32 喂数据(每秒一帧,循环 4 类业务帧):
```bash
cd experiments/m5_parser
g++ -std=c++17 -Wall CRC16.cpp fake_stm32.cpp -o fake_stm32
./fake_stm32 /tmp/ttyV1
```

### 验证

浏览器打开 **http://localhost:8888** —— 实时设备卡片 + 点卡片切换 uPlot 历史曲线。

命令行交叉验证:
```bash
# SQLite 落库
sqlite3 /tmp/gateway.db "SELECT * FROM device_data ORDER BY ts DESC LIMIT 5;"

# MQTT 上行
mosquitto_sub -h localhost -t 'gateway/up/#' -v

# HTTP API(只读连接查询,按 ts 倒序)
curl 'http://localhost:8888/api/data?dev=temperature&n=10'
```

业务解码(`decodeFrame`)产出的设备:`temperature` / `humidity`(温湿度帧拆两条)、`illuminance`(光照)、`device_status`(状态);心跳帧不落库。

详细协议格式参见 [docs/m5_frame_protocol.md](docs/m5_frame_protocol.md)。

## License

(待定)
