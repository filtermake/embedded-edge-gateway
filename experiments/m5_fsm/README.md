# experiments/m5_fsm/

门禁刷卡系统 FSM 练习项目(M5 学习过程中的概念验证)。

**主项目无关**:这是 M5 学习阶段为了熟悉 FSM 三件套(状态/事件/转移)写的练习。
真正的 M5 协议解析器在 `src/protocol/`,本目录仅作学习归档。

## 内容

4 状态门禁 FSM:`IDLE → VERIFYING → OPEN/ALARM`,用 switch-case 实现,演示 FSM 设计原则:
- 状态转移表设计
- 非法事件的兜底处理
- 完备性(每个状态 × 每个事件都有明确响应)
- 健壮性(异常不让 FSM 卡死)

## 跑法

```bash
g++ -std=c++17 -Wall AccessControlFSM.cpp main.cpp -o fsm_demo
./fsm_demo
```
