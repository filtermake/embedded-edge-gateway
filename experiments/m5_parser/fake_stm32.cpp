// fake_stm32.cpp - 模拟 STM32 端发送 M5 协议帧
#include "CRC16.h"

#include <fcntl.h>          // open
#include <unistd.h>         // write, close, sleep
#include <termios.h>        // tcsetattr (PTY 也是 termios 设备)

#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

// 工具函数:构造一个完整 M5 协议帧
std::vector<uint8_t> buildFrame(uint8_t type, const std::vector<uint8_t>& payload) {
    std::vector<uint8_t> frame;
    frame.reserve(2 + 1 + 1 + payload.size() + 2);
    
    // 帧头
    frame.push_back(0xAA);
    frame.push_back(0x55);
    
    // LEN = TYPE 字节 + payload data 字节
    uint8_t len = static_cast<uint8_t>(1 + payload.size());
    frame.push_back(len);
    
    // TYPE
    frame.push_back(type);
    
    // payload data
    for (auto b : payload) frame.push_back(b);
    
    // CRC16 计算:范围是 LEN + TYPE + payload data
    m5::CRC16 crc;
    crc.update(len);
    crc.update(type);
    for (auto b : payload) crc.update(b);
    uint16_t crc_val = crc.value();
    
    // 小端写入
    frame.push_back(static_cast<uint8_t>(crc_val & 0xFF));         // CRC_LO
    frame.push_back(static_cast<uint8_t>((crc_val >> 8) & 0xFF));  // CRC_HI
    
    return frame;
}

// 工具函数:把字节流以 hex 形式打印出来(debug 用)
void printHex(const std::vector<uint8_t>& data) {
    for (auto b : data) {
        printf("%02X ", b);
    }
    printf("\n");
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <pty_path>\n";
        std::cerr << "Example: " << argv[0] << " /dev/pts/3\n";
        return 1;
    }
    
    const char* pty_path = argv[1];
    
    // 打开 PTY(用法和打开真实串口一样)
    int fd = open(pty_path, O_WRONLY | O_NOCTTY);
    if (fd < 0) {
        perror("open");
        return 1;
    }
    
    std::cout << "[fake_stm32] Connected to " << pty_path << "\n";
    std::cout << "[fake_stm32] Sending frames every 1 second...\n\n";
    
    // 构造几个测试帧
    std::vector<std::pair<std::string, std::vector<uint8_t>>> frames = {
        {"心跳",     buildFrame(0x03, {})},
        {"光照",     buildFrame(0x02, {0x01, 0x90})},          // 400 lux
        {"温湿度",   buildFrame(0x01, {0x00, 0xFD, 0x02, 0x5D, 0x00})},  // 25.3°C 60.5%
        {"状态",     buildFrame(0x04, {0x01})},
    };
    
    // 循环发送
    int counter = 0;
    while (true) {
        for (auto& [name, frame] : frames) {
            std::cout << "[fake_stm32] #" << counter++ 
                      << " sending [" << name << "] " 
                      << frame.size() << " bytes: ";
            printHex(frame);
            
            ssize_t n = write(fd, frame.data(), frame.size());
            if (n < 0) {
                perror("write");
                close(fd);
                return 1;
            }
            
            sleep(1);  // 间隔 1 秒
        }
    }
    
    close(fd);
    return 0;
}