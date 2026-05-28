# Protocol 模块:CRC16 + 8 状态 FSM 协议解析器
# 详见 docs/m5_frame_protocol.md
add_library(protocol STATIC
    CRC16.cpp
    FrameParser.cpp
)

# 头文件路径暴露给依赖者
target_include_directories(protocol PUBLIC .)

# 应用统一编译选项
target_link_libraries(protocol PRIVATE
    gateway_options
)
