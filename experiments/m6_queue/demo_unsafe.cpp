#include <iostream>
#include <queue>
#include <thread>

constexpr int N = 100000;
std::queue<int> q;

void producer() {
    for (int i = 0; i < N; i++) {
        q.push(i);
    }
}

void consumer() {
    int popped = 0;
    while (popped < N) {
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