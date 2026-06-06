# Config 模块:配置解析 + 热加载管理(load-then-swap + atomic shared_ptr)
add_library(config STATIC
    Config.cpp
)
target_include_directories(config PUBLIC .)
target_link_libraries(config
    PRIVATE gateway_options
    PRIVATE  log            # Config 用 LOG_WARN,且头文件不暴露 log,本可 PRIVATE;
)                          # 但 reload/validate 实现依赖 Logger,PRIVATE 即可,见下注