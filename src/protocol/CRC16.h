// CRC16.h
#pragma once
#include <cstdint>

namespace gateway {

class CRC16 {
public:
    CRC16() { reset(); }

    // 重置累加器(每帧开始时调用)
    void reset() noexcept { value_ = 0xFFFF; }

    // 累加 1 字节
    void update(uint8_t byte) noexcept;

    // 取当前 CRC 值
    uint16_t value() const noexcept { return value_; }

private:
    uint16_t value_;
};

} // namespace gateway
