#include "ThreadSafeQueue.h"
#include <iostream>
#include <thread>
#include <chrono>

m6::ThreadSafeQueue<int> tsq;

void consumer(int id) {
    int count = 0;
    while (auto item = tsq.pop()) {   // 拿到 nullopt 就退出
        ++count;
    }
    std::cout << "consumer " << id << " exit, consumed " << count << "\n";
}

int main() {
    std::thread c1(consumer, 1);
    std::thread c2(consumer, 2);     // 故意开 2 个,验证 notify_all

    for (int i = 0; i < 10; ++i) tsq.push(i);
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // 让消费者把数据吃完,然后睡在 wait 上

    std::cout << "main: calling shutdown\n";
    tsq.shutdown();                  // 关闭 → 两个消费者都应被唤醒退出

    c1.join();                       // 如果 shutdown 没写对,这里会永久卡住
    c2.join();
    std::cout << "main: all consumers joined, clean exit\n";
    return 0;
}