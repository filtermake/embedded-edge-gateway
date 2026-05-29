#include "ThreadSafeQueue.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>

int main() {
    m6::ThreadSafeQueue<int> q(3);   // ← 容量只有 3,很容易满
    std::atomic<int> produced{0};    // 记录生产了多少(atomic 避免自己又引入 race)

    // 生产者:狂塞 10 个
    std::thread prod([&] {
        for (int i = 0; i < 10; ++i) {
            q.push(i);
            ++produced;
            std::cout << "pushed " << i << ", produced so far=" << produced.load() << "\n";
        }
        std::cout << "producer done\n";
    });

    // 故意让消费者慢:启动前先睡 1 秒,让生产者先把队列塞满、卡在 push 上
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::cout << "--- consumer starts now ---\n";

    std::thread cons([&] {
        for (int i = 0; i < 10; ++i) {
            auto v = q.pop();
            std::cout << "  popped " << *v << "\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(200)); // 消费慢
        }
    });

    prod.join();
    cons.join();
    return 0;
}