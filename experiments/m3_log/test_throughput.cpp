#include "AsyncLogger.h"
#include <thread>
#include <vector>
#include <atomic>
#include <iostream>
#include <chrono>
#include <cstdio>

int main() {
    m3::AsyncLogger logger("/tmp/async_log_test.log");

    constexpr int N_THREADS = 4;
    constexpr int N_PER_THREAD = 50000;

    auto start = std::chrono::steady_clock::now();

    std::vector<std::thread> threads;
    for (int t = 0; t < N_THREADS; ++t) {
        threads.emplace_back([&logger, t]{
            char buf[128];
            for (int i = 0; i < N_PER_THREAD; ++i) {
                int n = snprintf(buf, sizeof(buf),
                                 "thread=%d seq=%d hello world from async logger\n",
                                 t, i);
                logger.append(buf, n);
            }
        });
    }
    for (auto& th : threads) th.join();

    auto end = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    int total = N_THREADS * N_PER_THREAD;
    std::cout << "submitted " << total << " logs in " << ms << " ms\n";
    std::cout << "throughput: " << (total * 1000LL / std::max<long long>(ms, 1))
              << " logs/sec (front-end perceived)\n";

    // logger 在这里析构,后台线程把剩余 buffer 刷盘
    return 0;
}