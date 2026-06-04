#include "Logger.h"
#include "SerialPort.h"
#include "FrameParser.h"
#include "ThreadPool.h"
#include "Database.h"
#include "MqttClient.h"

#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <exception>
#include <string>
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

    // Database 必须先于 ThreadPool 构造:线程池任务会引用 db,而 ~ThreadPool
    // 析构时要 drain 在飞任务。声明在前 => 构造在前、析构在后,保证 drain 时 db 仍存活。
    gateway::Database db("/tmp/gateway.db");
    LOG_INFO("%s", "sqlite opened at /tmp/gateway.db");

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

        // MQTT 上云链路:收到消息后丢进线程池写库。broker 连不上时构造抛异常,
        // 被外层 catch 捕获 => 网关启动失败退出(致命语义)。
        // ~MqttClient 在 try 作用域退出时先于 pool 析构 => 停后台线程、不再提交任务。
        gateway::MqttClient client("gateway-main", "localhost", 1883, 60);
        client.setMessageHandler([&db, &pool](const std::string& topic,
                                              const std::string& payload) {
            // topic/payload 按值捕进任务:回调返回后原串即失效,引用会悬垂。
            pool.submit([&db, topic, payload] {
                double v = 0;
                try { v = std::stod(payload); } catch (...) { return; }
                std::string dev = topic;
                size_t pos = topic.find_last_of('/');
                if (pos != std::string::npos) dev = topic.substr(pos + 1);
                db.insert(dev, v, static_cast<long>(time(nullptr)));
            });
        });
        client.subscribe("gateway/sensor/#", 1);
        client.loopStart();
        LOG_INFO("%s", "mqtt connected, subscribed gateway/sensor/#");

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
