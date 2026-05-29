# Concurrent 模块:线程安全队列 + 线程池
# 两者均为模板类(header-only),所以是 INTERFACE 库

add_library(concurrent INTERFACE)
target_include_directories(concurrent INTERFACE .)

# std::thread 在 Linux 上需要链接 pthread
# Modern CMake 通过 Threads::Threads 抽象处理(可移植)
find_package(Threads REQUIRED)
target_link_libraries(concurrent INTERFACE Threads::Threads)
