#include <iostream>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>

constexpr int N = 1000;
std::queue<int> q;

std::mutex mut;
std::condition_variable cv;

void producer() {
    for (int i = 0; i < N; i++) {
        std::unique_lock<std::mutex> lock(mut);
        q.push(i);
        lock.unlock();
        cv.notify_one();
        std::this_thread::sleep_for(std::chrono::milliseconds(1)); // 每 1ms 才产一个
    }
}

void consumer() {
    int popped = 0;
    while (popped < N) {
        std::unique_lock<std::mutex> lock(mut);
        cv.wait(lock, [](){return !q.empty();});
        q.pop();
        popped++;
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