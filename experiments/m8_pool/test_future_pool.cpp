#include "ThreadPool.h"
#include <iostream>
#include <future>
#include <vector>

int main() {
    m8::ThreadPool pool(4);

    // 提交 8 个"算平方"任务,收集 future
    std::vector<std::future<int>> futures;
    for (int i = 1; i <= 8; ++i) {
        futures.push_back(pool.submit([i]{
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            return i * i;
        }));
    }

    // 依次取结果(get 会阻塞到对应任务完成)
    for (int i = 0; i < 8; ++i) {
        std::cout << "result[" << i << "] = " << futures[i].get() << "\n";
    }

    auto f = pool.submit([]() -> int { throw std::runtime_error("boom"); });
    try {
        f.get();
    } catch (const std::exception& e) {
        std::cout << "caught: " << e.what() << "\n";
    }
    return 0;
}