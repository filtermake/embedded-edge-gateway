#include <iostream>
#include <queue>
#include <thread>
#include <mutex>

constexpr int N = 1000;
std::queue<int> q;

std::mutex mut;

void producer() {
    for (int i = 0; i < N; i++) {
        std::lock_guard<std::mutex> lock(mut);
        q.push(i);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void consumer() {
    int popped = 0;
    while (popped < N) {
        std::lock_guard<std::mutex> lock(mut);
        if (!q.empty()) {
            q.pop();
            ++popped;
        }
    }
    std::cout << "consumer done, popped=" << popped << "\n";
}

int main() {
    std::thread t1(producer);
    std::thread t2(consumer);
    t1.join();
    t2.join();
    std::cout << "remaining in queue: " << q.size() << "\n";
    return 0;
}