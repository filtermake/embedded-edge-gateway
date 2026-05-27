// main.cpp
#include "FrameParser.h"
#include <iostream>
#include <iomanip>
#include <memory>

int main() {
    auto makeParser = []() {
        auto p = std::make_unique<m5::FrameParser>();
        p->setOnFrame([](const m5::Frame& f) {
            std::cout << "  [✓ FRAME RECEIVED] type=0x" 
                      << std::hex << std::setw(2) << std::setfill('0')
                      << static_cast<int>(f.type) << std::dec
                      << ", payload size=" << f.payload.size();
            if (!f.payload.empty()) {
                std::cout << ", data=";
                for (auto b : f.payload) {
                    std::cout << std::hex << std::setw(2) << std::setfill('0')
                              << static_cast<int>(b) << " ";
                }
                std::cout << std::dec;
            }
            std::cout << "\n";
        });
        return p;
    };

    // -------- Test 1: 心跳帧,LEN=1, TYPE=0x03 --------
    // 帧数据: AA 55 01 03 [CRC]
    // CRC 计算范围: 01 03
    // 用你的 CRC16 实现算出 CRC = 0x817E (cross-check with: result of update(0x01), update(0x03))
    {
        std::cout << "--- Test 1: 心跳帧(LEN=1, TYPE=0x03) ---\n";
        auto parser = makeParser();
        uint8_t frame[] = { 0xAA, 0x55, 0x01, 0x03, 0x40, 0x21 };
        for (auto b : frame) parser->feed(b);
    }

    // -------- Test 2: BH1750 光照帧,LEN=3 --------
    // 帧数据: AA 55 03 02 01 90 [CRC]
    // CRC 范围: 03 02 01 90
    // CRC = 0xFA5B (算出来后填,你跑下面 Test 5 自动验证)
    {
        std::cout << "\n--- Test 2: BH1750 光照帧(LEN=3, TYPE=0x02, payload=01 90) ---\n";
        auto parser = makeParser();
        uint8_t frame[] = { 0xAA, 0x55, 0x03, 0x02, 0x01, 0x90, 0xa0, 0x5c };
        for (auto b : frame) parser->feed(b);
    }

    // -------- Test 3: CRC 错误 --------
    {
        std::cout << "\n--- Test 3: CRC 错误的帧(故意改 CRC 高字节) ---\n";
        auto parser = makeParser();
        uint8_t frame[] = { 0xAA, 0x55, 0x03, 0x02, 0x01, 0x90, 0x5B, 0xFF };  // CRC HI 改错
        for (auto b : frame) parser->feed(b);
    }

    // -------- Test 4: 连续两帧 --------
    {
        std::cout << "\n--- Test 4: 连续两帧(心跳 + 光照) ---\n";
        auto parser = makeParser();
        uint8_t frame1[] = { 0xAA, 0x55, 0x01, 0x03, 0x40, 0x21 };
        uint8_t frame2[] = { 0xAA, 0x55, 0x03, 0x02, 0x01, 0x90, 0xa0, 0x5c };
        for (auto b : frame1) parser->feed(b);
        for (auto b : frame2) parser->feed(b);
    }

    // -------- Test 5: 帧前有垃圾字节 --------
    {
        std::cout << "\n--- Test 5: 帧前有垃圾字节(干扰场景模拟) ---\n";
        auto parser = makeParser();
        uint8_t junk[] = { 0xFF, 0x00, 0x12, 0xAB };
        uint8_t frame[] = { 0xAA, 0x55, 0x01, 0x03, 0x40, 0x21 };
        for (auto b : junk) parser->feed(b);
        for (auto b : frame) parser->feed(b);
    }

    return 0;
}