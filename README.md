# embedded-edge-gateway

C++17 多协议嵌入式 Linux 边缘网关,目标运行环境:Raspberry Pi 4B。

## 模块状态

| 模块 | 路径 | 状态 |
|---|---|---|
| M3 日志 | `src/log/` | ✅ Logger 同步外壳 + AsyncLogger 双缓冲后端 / Meyers 单例 / 后台 flush 线程(3 秒间隔)/ 落盘 `/tmp/gateway.log` |
| M4 串口 | `src/serial/` | ✅ termios RAII 类 / Rule of Five / 8N1 / VMIN=1 VTIME=0 |
| M5 协议 | `src/protocol/` | ✅ 8 状态 FSM + CRC16-MODBUS,详见 [docs/m5_frame_protocol.md](docs/m5_frame_protocol.md) |
| M6 队列 | `src/concurrent/ThreadSafeQueue.h` | ✅ 模板类 / mutex + 双 cv / 有界容量 / shutdown 排空语义 |
| M8 线程池 | `src/concurrent/ThreadPool.h` | ✅ 基于 M6 队列 / `submit()` 返回 `std::future` / RAII join 工作线程 |
| M7 Reactor | `src/net/` | ✅ epoll(ET)事件循环 + channel RAII(禁拷贝 / move)/ 读写回调,echo_server 示例 |
| M9 定时器 | `src/net/timer_server.cpp` | ✅ timerfd 周期定时 + 空闲连接超时踢出 |
| M11 HTTP | `src/net/HttpRequest.h` | ✅ Buffer 双游标 + HTTP 四状态 FSM(请求行 / 头 / body)/ keep-alive 超时,curl 端到端 + 单测 7 条 |
| M12 SQLite | `src/db/` | ✅ Statement RAII + Database(连接锁 + 缓存 prepared insert + WAL) |
| M10 MQTT | `src/mqtt/` | ✅ MqttClient RAII 封装 libmosquitto,std::function 消息回调 |
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

## Phase 1 Demo(M4 串口 + M5 协议解析端到端)

需要 3 个终端,演示 STM32 节点(模拟)→ 树莓派网关的完整数据链路。

**终端 1 — 启动虚拟串口对**:
```bash
./scripts/start_vserial.sh
```
保持运行不要关。会创建 `/tmp/ttyV0`(网关侧)和 `/tmp/ttyV1`(STM32 侧)。

**终端 2 — 启动网关**:
```bash
./build/gateway /tmp/ttyV0
```
启动后阻塞等待帧到来。

**终端 3 — 启动假 STM32**:

先编译 experiments 下的 fake_stm32:
```bash
cd experiments/m5_parser
g++ -std=c++17 -Wall CRC16.cpp fake_stm32.cpp -o fake_stm32
./fake_stm32 /tmp/ttyV1
```

**预期网关输出**(每秒一帧,循环 4 类业务帧):
```
[FRAME] type=0x03 payload[0]=             (心跳)
[FRAME] type=0x02 payload[2]=01 90        (光照 = 400 lux)
[FRAME] type=0x01 payload[5]=00 FD 02 5D 00  (温湿度 = 25.3°C 60.5%)
[FRAME] type=0x04 payload[1]=01           (设备状态)
...
```

详细协议格式参见 [docs/m5_frame_protocol.md](docs/m5_frame_protocol.md)。

## Phase 2 Demo(M10 MQTT + M12 SQLite 上云落库)

`gateway` 在串口链路之外并行跑一条 MQTT 上云链路:
**MQTT 订阅 `gateway/sensor/#` → 线程池 → SQLite 落库 `/tmp/gateway.db`**。

- 串口仍是主线程阻塞 `read` 循环(数据来自 `fake_stm32`),MQTT 在后台线程 `loopStart`。
- 两条链路共用同一个 `ThreadPool`;`Database` 声明在线程池之前,保证析构时在飞任务仍能安全写库。
- broker 连不上视为致命错误,网关启动失败退出。

需要先启动 broker 和虚拟串口(串口必需,否则网关退出):
```bash
sudo systemctl start mosquitto          # 或 mosquitto -c /etc/mosquitto/mosquitto.conf
./scripts/start_vserial.sh              # 另开终端,保持运行
./build/gateway /tmp/ttyV0              # 另开终端启动网关
```

发布一条传感器数据并查询落库结果:
```bash
mosquitto_pub -h localhost -t gateway/sensor/temp -m 25.3
sqlite3 /tmp/gateway.db "SELECT * FROM device_data;"
# 1|temp|25.3|<ts>
```

device_id 取 topic 最后一段;payload 解析失败(非数字)的消息会被安全跳过。

## License

(待定)
