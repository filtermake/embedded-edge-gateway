# embedded-edge-gateway

C++17 多协议嵌入式 Linux 边缘网关,目标运行环境:Raspberry Pi 4B。

## 模块状态

| 模块 | 路径 | 状态 |
|---|---|---|
| M3 日志 | `src/log/` | 雏形(简易宏,Phase 2 升级) |

## Build

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug
cmake --build build
./build/gateway
```

## License

(待定)
