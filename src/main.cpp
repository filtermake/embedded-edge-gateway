#include "Logger.h"
#include "SerialPort.h"
#include "FrameParser.h"

#include <cerrno>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <exception>
#include <unistd.h>
#include <termios.h>

int main(int argc, char* argv[]) {
    const char* port_path = (argc > 1) ? argv[1] : "/tmp/ttyV0";

    LOG_INFO("gateway %s starting on %s", "v0.1.0", port_path);

    try {
        gateway::SerialPort port(port_path, B115200);
        LOG_INFO("serial opened, fd=%d", port.get());

        gateway::FrameParser parser;
        parser.setOnFrame([](const gateway::Frame& f) {
            printf("[FRAME] type=0x%02X payload[%zu]=", f.type, f.payload.size());
            for (auto b : f.payload) printf("%02X ", b);
            printf("\n");
        });

        // 主循环:从串口 read 字节,逐字节 feed 给 FSM
        uint8_t buf[64];
        while (true) {
            ssize_t n = read(port.get(), buf, sizeof(buf));
            if (n < 0) {
                if (errno == EINTR) continue;          // 被信号打断,重试
                LOG_ERROR("read failed: %s", strerror(errno));
                return 1;
            }
            if (n == 0) {
                LOG_INFO("%s", "EOF, exiting");
                break;
            }
            for (ssize_t i = 0; i < n; ++i) {
                parser.feed(buf[i]);
            }
        }
    } catch (const std::exception& e) {
        LOG_ERROR("gateway error: %s", e.what());
        return 1;
    }

    return 0;
}
