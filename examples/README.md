# LIBLSX 模块使用示例集

本目录为库的 **17 个模块** 各提供一个可编译、可运行的示例程序（`example_<模块>.cpp`），
每个示例都带详细的中文注释，演示该模块最常用的 API 用法、典型场景与注意事项。

## 示例清单

| 示例文件 | 模块 | 演示内容 |
|----------|------|----------|
| `example_thread.cpp` | Thread | ThreadWrapper 线程生命周期、Scheduler 一次性/周期任务、ThreadPool 并发任务 |
| `example_datatransfer.cpp` | DataTransfer | TCP/UDP 客户端服务器回环、工厂模式 |
| `example_memory.cpp` | Memory | Buffer/Pipe/FIFO/CircularQueue/SharedMemory |
| `example_lock.cpp` | LockManager | LockGuard/SharedLockGuard/MultiLockGuard/Condition |
| `example_logger.cpp` | Logger | 级别过滤、控制台/文件输出、日志轮转 |
| `example_timer.cpp` | Timer | TimerWheel 一次性/周期定时器、Stopwatch、Deadline |
| `example_eventbus.cpp` | EventBus | 类型安全发布-订阅、RAII 句柄、嵌套分发 |
| `example_serialize.cpp` | Serialize | 定长整数/varint/TLV 编解码、失败不抛异常 |
| `example_ipc.cpp` | IPC | Unix domain socket 服务端/客户端回环 |
| `example_result.cpp` | Result | 值/错误二者取一、valueOr、map、void 特化 |
| `example_bus.cpp` | Bus | I2C/SPI 总线（注入 fd 免硬件演示） |
| `example_protocol.cpp` | Protocol | 帧头帧尾/长度字段流式帧解析 |
| `example_checksum.cpp` | Checksum | CRC8/16/32、sum/xor/Fletcher 增量校验 |
| `example_crypto.cpp` | Crypto | AES/SHA-256/HMAC/Base64/Hex |
| `example_compress.cpp` | Compress | DEFLATE/zlib/gzip/RLE 压缩解压 |
| `example_config.cpp` | Config | ConfigStore INI 读写、全局变量绑定、变更回调 |
| `example_statemachine.cpp` | StateMachine | 泛型 FSM、守卫/动作、层次状态、超时、DOT 导出 |

## 构建与运行

### 方式一：挂进顶层 CMake（推荐）

```bash
cd <库根目录>
mkdir -p build && cd build
cmake .. -DCOMPILER_TARGET=x86 -DTARGET_TYPE=both -DBUILD_EXAMPLES=ON
make example_all -j          # 一键构建全部 17 个示例
./x86/bin/example_timer      # 运行单个示例
```

### 方式二：独立脚本（不依赖顶层 CMake）

```bash
cd examples
./build_all.sh                # 先编库，再逐个编示例到 ./bin/
./bin/example_timer
```

> 说明：示例默认与静态库 `lsx_static` 链接；Thread 相关示例需要 `-lpthread`。

## 阅读建议

- 每个示例都是**自包含**的：`main()` 只演示一个模块，注释直接写在代码行间。
- 注释分三层：文件头说明模块定位 → 每个演示段落前的"场景说明" → 关键语句的行内解释。
- 涉及硬件/网络的示例（Bus、DataTransfer、IPC）都采用**可注入 fd / 回环地址**，
  无需真实设备或外网即可运行，便于 CI 与离线验证。

## 与 example/ 文档目录的关系

- `examples/`（本目录）：**可运行的代码示例**，带行内注释。
- `example/`（库根目录）：每个模块的 **Markdown 使用说明文档**。

两者互补：先读 Markdown 了解概念与接口全貌，再跑本目录的示例看真实调用。
