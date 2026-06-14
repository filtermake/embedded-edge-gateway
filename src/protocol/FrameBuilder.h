// FrameBuilder.h
#pragma once
#include <cstdint>
#include <vector>

namespace gateway {

// 把 type + payload 组装成完整 M5 帧(含帧头 AA 55、LEN、CRC 小端)。
// payload 里应已包含 seq(由调用方/上层命令管理负责),本函数不分配 seq。
// 返回完整帧字节,可直接交给 SerialPort::write 发送。
std::vector<uint8_t> buildFrame(uint8_t type, const std::vector<uint8_t>& payload);

}