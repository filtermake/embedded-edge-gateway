// gateway.cpp - 真网关程序,M4 串口 + M5 协议解析
#include "FrameParser.h"
#include "CRC16.h"

#include <fcntl.h>
#include <unistd.h>
#include <termios.h>

#include <cstdio>
#include <cstdint>
#include <iostream>
#include <vector>

// 简化版的"串口打开"(M4 的精简版,够用即可)
// 真实 M5 集成会用你 M4 写的 SerialPort RAII 类
int openSerial(const char* path) {
    int fd = open(path, O_RDONLY | O_NOCTTY);
    if (fd < 0) {
        perror("open");
        return -1;
    }
    
    // PTY 默认就是 raw 模式(socat 启动时指定了),
    // 这里不再做 termios 配置,简化
    
    return fd;
}

// 帧接收回调:打印帧内容
void onFrame(const m5::Frame& f) {
    std::cout << "  [✓ FRAME] type=0x";
    printf("%02X", f.type);
    std::cout << " payload[" << f.payload.size() << "]=";
    for (auto b : f.payload) {
        printf("%02X ", b);
    }
    std::cout << "\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <pty_path>\n";
        std::cerr << "Example: " << argv[0] << " /dev/pts/4\n";
        return 1;
    }
    
    const char* pty_path = argv[1];
    
    int fd = openSerial(pty_path);
    if (fd < 0) return 1;
    
    std::cout << "[gateway] Listening on " << pty_path << "\n";
    std::cout << "[gateway] Waiting for frames...\n\n";
    
    // 初始化 M5 解析器,注册回调
    m5::FrameParser parser;
    parser.setOnFrame(onFrame);
    
    // 主循环:从串口读字节,喂给解析器
    uint8_t buf[64];
    while (true) {
        ssize_t n = read(fd, buf, sizeof(buf));
        if (n < 0) {
            perror("read");
            break;
        }
        if (n == 0) {
            // EOF(对端关闭)
            std::cout << "[gateway] EOF, exiting\n";
            break;
        }
        
        // 把读到的 n 个字节逐个喂给 FSM
        for (ssize_t i = 0; i < n; ++i) {
            parser.feed(buf[i]);
        }
    }
    
    close(fd);
    return 0;
}