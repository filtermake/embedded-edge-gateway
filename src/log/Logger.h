#pragma once
#include <cstdio>

// 简易日志宏(Phase 0 雏形)
// Phase 2 升级为异步双缓冲版,加 LogLevel 过滤、线程安全、刷盘策略

#define LOG_INFO(fmt, ...) \
    fprintf(stderr, "[INFO ] %s:%d " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)

#define LOG_WARN(fmt, ...) \
    fprintf(stderr, "[WARN ] %s:%d " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)

#define LOG_ERROR(fmt, ...) \
    fprintf(stderr, "[ERROR] %s:%d " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
