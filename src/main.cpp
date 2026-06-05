#include "Logger.h"
#include "SerialPort.h"
#include "FrameParser.h"
#include "ThreadPool.h"
#include "Database.h"
#include "MqttClient.h"
#include "HttpServer.h"             // [S5改] 内嵌 HTTP 监控服务

#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <exception>
#include <string>
#include <vector>
#include <thread>                   // [S5改] HTTP 线程
#include <unistd.h>
#include <termios.h>


// ============ 业务解码层:Frame → Record ============
struct Record {
    std::string device_id;
    double      value;
};

namespace frame_type {
    constexpr uint8_t kDHT11     = 0x01;
    constexpr uint8_t kBH1750    = 0x02;
    constexpr uint8_t kHeartbeat = 0x03;
    constexpr uint8_t kStatus    = 0x04;
}

std::vector<Record> decodeFrame(const gateway::Frame& f) {
    std::vector<Record> out;
    const auto& p = f.payload;

    switch (f.type) {
    case frame_type::kDHT11: {
        if (p.size() < 5) {
            LOG_WARN("DHT11 frame payload too short: %zu", p.size());
            return out;
        }
        double temperature = ((static_cast<uint16_t>(p[0]) << 8) | p[1]) / 10.0;
        double humidity    = ((static_cast<uint16_t>(p[2]) << 8) | p[3]) / 10.0;
        out.emplace_back(Record{"temperature", temperature});
        out.emplace_back(Record{"humidity",    humidity});
        // TODO: 可校验 DHT11 checksum p[4]
        break;
    }
    case frame_type::kBH1750: {
        if (p.size() < 2) {
            LOG_WARN("BH1750 frame payload too short: %zu", p.size());
            return out;
        }
        double illuminance = (static_cast<uint16_t>(p[0]) << 8) | p[1];
        out.emplace_back(Record{"illuminance", illuminance});
        break;
    }
    case frame_type::kStatus: {
        if (p.size() < 1) return out;
        out.emplace_back(Record{"device_status", static_cast<double>(p[0])});
        break;
    }
    case frame_type::kHeartbeat:
        break;
    default:
        LOG_WARN("unknown frame type: 0x%02X", f.type);
        break;
    }
    return out;
}

int main(int argc, char* argv[]) {
    const char* port_path = (argc > 1) ? argv[1] : "/tmp/ttyV0";

    LOG_INFO("gateway %s starting on %s", "v0.1.0", port_path);

    // RAII 对象统一放 try 内。声明顺序 db -> client -> roDb -> (http_thread) -> pool。
    //   - pool 声明最后 => 最先析构(drain 在飞任务),此时 db/client 仍存活,无 UAF。
    //   - 构造异常(磁盘/broker/线程)统一被下方 catch 处理 => 优雅致命退出。
    try {
        // 1) 读写连接(主链 worker 落库依赖)
        gateway::Database db("/tmp/gateway.db");
        LOG_INFO("%s", "sqlite(rw) opened at /tmp/gateway.db");

        // 2) MQTT 客户端(主链 worker 上行发布依赖)
        gateway::MqttClient client("gateway-main", "localhost", 1883, 60);
        client.setMessageHandler([](const std::string& topic,
                                    const std::string& payload) {
            LOG_INFO("downlink cmd recv: topic=%s payload=%s (not handled yet)",
                     topic.c_str(), payload.c_str());
        });
        client.subscribe("gateway/cmd/#", 1);
        client.loopStart();
        LOG_INFO("%s", "mqtt connected, subscribed gateway/cmd/#");

        // 3) [S5改] 只读连接 + HTTP 监控线程。
        //    roDb 是独立的【只读】sqlite 连接(SQLITE_OPEN_READONLY):
        //      - 与主链写连接 db 物理上是两个连接,靠 WAL 实现读写并发(查询不阻塞落库)。
        //      - 最小权限:HTTP 接口从连接层就无写能力。
        //    HTTP 线程跑永久阻塞的 epoll loop,故 detach。
        //    【局限】detach 依赖"进程整体被 kill 时所有线程一起终止",roDb 生命周期
        //    靠进程同生共死隐式保证。优雅退出(信号->停loop->join->回收)留待 M1 守护进程。
        gateway::Database roDb("/tmp/gateway.db", true);
        std::thread http_thread([&roDb]{ gateway::runHttpServer(roDb); });
        http_thread.detach();
        LOG_INFO("%s", "http monitor thread started on :8888");

        // 4) 线程池(声明最后 => 最先析构,drain 时 db/client 仍活着)
        gateway::ThreadPool pool(4);
        LOG_INFO("%s", "thread pool started (4 workers)");

        // 串口 + 协议解析
        gateway::SerialPort port(port_path, B115200);
        LOG_INFO("serial opened, fd=%d", port.get());

        gateway::FrameParser parser;
        parser.setOnFrame([&pool, &db, &client](const gateway::Frame& f) {
            LOG_DEBUG("frame type=0x%02X payload_len=%zu", f.type, f.payload.size());

            long ts = static_cast<long>(time(nullptr));   // 采集时刻
            std::vector<Record> records = decodeFrame(f);

            for (const auto& r : records) {
                std::string dev = r.device_id;
                double      val = r.value;
                // 双写:落本地库 + 上行发布。db/client 生命周期长 => 引用捕获
                pool.submit([&db, &client, dev, val, ts] {
                    db.insert(dev, val, ts);
                    client.publish("gateway/up/" + dev, std::to_string(val));
                });
            }
        });

        // 主循环:串口 read -> 逐字节 feed FSM
        uint8_t buf[64];
        while (true) {
            ssize_t n = read(port.get(), buf, sizeof(buf));
            if (n < 0) {
                if (errno == EINTR) continue;
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