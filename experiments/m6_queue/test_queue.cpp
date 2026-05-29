#include "ThreadSafeQueue.h"
#include <thread>
#include <iostream>

constexpr int N = 100000;
m6:: ThreadSafeQueue<int> tsq;

void producer() {
    for (int i = 0; i < N; i++) tsq.push(i);
}

void consumer() {
    long long sum = 0;
    for (int i = 0; i < N; ++i) {
        int v = tsq.pop();      // 队空会自动阻塞等待
        sum += v;
    }

    // 验证:0+1+...+(N-1) = N*(N-1)/2
    long long expect = (long long)N * (N - 1) / 2;
    std::cout << "sum=" << sum << " expect=" << expect
              << (sum == expect ? "  PASS" : "  FAIL") << "\n";
}

int main() {
    std::thread t1(producer);
    std::thread t2(consumer);
    t1.join();
    t2.join();
    return 0;
}