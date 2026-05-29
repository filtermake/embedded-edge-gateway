#include "ThreadPool.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>

int main() {
    {                                    // ← 加一个作用域,让 pool 在这里析构
        m8::ThreadPool pool(4);
        std::atomic<int> counter{0};
        for (int i = 0; i < 20; ++i) {
            pool.submit([i, &counter]{
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                ++counter;
            });
        }
        std::this_thread::sleep_for(std::chrono::seconds(3));  // 等任务跑完
        std::cout << "completed: " << counter.load() << " / 20\n";
    }   // ← pool 在这里析构:shutdown + join,干净退出
    std::cout << "pool destroyed cleanly\n";
    return 0;
}