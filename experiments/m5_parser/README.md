# experiments/m5_parser/

M5 协议解析器学习版,完整记录 P0→P3 增量开发过程(Walking Skeleton 模式)。

**主项目位置**:`src/protocol/` 是集成到主项目的版本(namespace 为 `gateway`)。
本目录保持 namespace 为 `m5`,作为学习过程归档。

## 内容

| 文件 | 作用 |
|---|---|
| `CRC16.{h,cpp}` | CRC16-MODBUS 实现(查表法 256 项) |
| `FrameParser.{h,cpp}` | 8 状态协议解析 FSM |
| `main.cpp` | 5 个单元测试场景(心跳/光照/CRC 错/连续帧/垃圾前缀) |
| `test_crc16.cpp` | CRC 标准测试向量验证 |
| `fake_stm32.cpp` | 模拟 STM32 节点(发送侧),用于 E2E 集成测试 |
| `gateway.cpp` | 简化版网关(接收侧),不依赖 M4 SerialPort 类 |

## 跑法

### M5 FSM 单元测试

```bash
g++ -std=c++17 -Wall CRC16.cpp FrameParser.cpp main.cpp -o m5_demo
./m5_demo
```

### CRC16 标准向量验证

```bash
g++ -std=c++17 -Wall CRC16.cpp test_crc16.cpp -o crc_test
./crc_test
# 期望:6 行全 PASS
```

### E2E 集成测试(socat 虚拟串口)

需要 3 个终端:

```bash
# 终端 1:启动虚拟串口
../../scripts/start_vserial.sh

# 终端 2:gateway 接收
g++ -std=c++17 -Wall CRC16.cpp FrameParser.cpp gateway.cpp -o gateway
./gateway /tmp/ttyV0

# 终端 3:fake_stm32 发送
g++ -std=c++17 -Wall CRC16.cpp fake_stm32.cpp -o fake_stm32
./fake_stm32 /tmp/ttyV1
```

详细协议格式参见 [../../docs/m5_frame_protocol.md](../../docs/m5_frame_protocol.md)。

## 与主项目的关系

主项目 `src/protocol/` 是从本目录迁移过去并改 namespace 为 `gateway` 的版本,
功能完全一致。任何代码修改应优先改主项目,本目录仅作为学习过程归档。
