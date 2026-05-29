#include "Logger.h"
#include "SerialPort.h"
#include "FrameParser.h"
#include "ThreadPool.h"

#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <exception>
#include <thread>
#include <unistd.h>
#include <termios.h>

// 模拟一段慢业务处理,让线程池的价值可观察(否则瞬时完成看不出区别)
static void process_frame(const gateway::Frame& f) {
    printf("[FRAME] type=0x%02X payload[%zu]=", f.type, f.payload.size());
    for (auto b : f.payload) printf("%02X ", b);
    printf("\n");
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

int main(int argc, char* argv[]) {
    const char* port_path = (argc > 1) ? argv[1] : "/tmp/ttyV0";

    LOG_INFO("gateway %s starting on %s", "v0.1.0", port_path);

    // 线程池先于 FrameParser 构造,保证生命周期覆盖所有在飞任务
    // (~ThreadPool 会 shutdown 队列并 join 工作线程,排空任务后才返回)
    gateway::ThreadPool pool(4);
    LOG_INFO("%s", "thread pool started (4 workers)");

    try {
        gateway::SerialPort port(port_path, B115200);
        LOG_INFO("serial opened, fd=%d", port.get());

        gateway::FrameParser parser;
        // 注意:Frame 必须按值捕获 —— onFrame 回调的 const Frame& 在 worker
        // 执行任务前就已离开 feed() 调用栈,按引用捕获会变成悬垂引用。
        parser.setOnFrame([&pool](const gateway::Frame& f) {
            pool.submit([f]{ process_frame(f); });
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
