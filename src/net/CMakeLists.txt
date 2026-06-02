# M7 Reactor:基于 epoll 的事件循环,用 STATIC 库
# M11 HTTP:HttpRequest 解析器并入 net 库
add_library(net STATIC
    EventLoop.cpp
    HttpRequest.cpp
)

# 头文件路径暴露给依赖者
target_include_directories(net PUBLIC .)

# 应用统一编译选项
target_link_libraries(net PRIVATE
    gateway_options
)

# echo server 示例:独立可执行,演示 Reactor 用法
add_executable(echo_server echo_server.cpp)
target_link_libraries(echo_server PRIVATE
    gateway_options
    net
)

# M9 timer server 示例:在 echo 基础上加 timerfd 定时器 + 空闲连接超时踢出
add_executable(timer_server timer_server.cpp)
target_link_libraries(timer_server PRIVATE
    gateway_options
    net
)

# M11 http server 示例:在 timer server 基础上加 HTTP 请求解析 + keep-alive
add_executable(http_server http_server.cpp)
target_link_libraries(http_server PRIVATE
    gateway_options
    net
)
