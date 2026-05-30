#include "AsyncLogger.h"
#include <thread>
#include <chrono>
#include <iostream>
#include <cstdio>
#include <cstdlib>

int main() {
    system("rm -f /tmp/timed_flush_test.log");                // ← 移到 logger 之前!
    m3::AsyncLogger logger("/tmp/timed_flush_test.log", 1);

    for (int i = 0; i < 5; ++i) {
        char buf[128];
        int n = snprintf(buf, sizeof(buf), "log line %d\n", i);
        logger.append(buf, n);
    }

    std::cout << "logged 5 lines, waiting 2 seconds...\n";
    std::this_thread::sleep_for(std::chrono::seconds(2));

    std::cout << "checking file before logger destruct:\n";
    system("cat /tmp/timed_flush_test.log");
    std::cout << "--- (lines above should appear) ---\n";
    return 0;
}