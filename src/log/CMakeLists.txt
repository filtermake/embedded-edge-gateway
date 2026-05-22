# 目前 log 模块只有宏(纯头文件),所以是 INTERFACE 库
# 注:Phase 2 学了"生产者-消费者"后,这里会升级为 STATIC 库
#     加入异步刷盘线程的 .cpp 实现

add_library(log INTERFACE)
target_include_directories(log INTERFACE .)
