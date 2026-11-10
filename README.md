# LIBLSX - 轻量级 C++ 跨平台工具库

[![MIT License](https://img.shields.io/badge/license-MIT-blue)](LICENSE)
![C++17](https://img.shields.io/badge/C++-17-blue.svg)

LIBLSX 是一个轻量级、跨平台的 C++ 工具库，旨在为开发者提供高效且易用的基础组件。当前已开发完成 **线程管理（Thread）**、**数据通信（DataTransfer）**、**内存管理（Memory）**、**锁管理（LockManager）** 和 **日志系统（Logger）** 五大核心模块。

## 功能特性

### 1. Thread 模块
- **线程生命周期管理**：通过 `ThreadWrapper` 实现线程的启动、停止、暂停、恢复。
- **灵活任务绑定**：支持 Lambda、自由函数、成员函数、函数对象等多种任务类型。
- **任务调度器**：`Scheduler` 支持一次性延迟任务和周期性任务调度。
- **线程池**：`ThreadPool` 管理多个工作线程并执行任务队列中的任务。
- **跨平台兼容**：基于 C++11 标准线程库，适配 Windows/Linux/macOS。

### 2. DataTransfer 模块
- **统一通信接口**：通过 `ICommunication` 抽象 UDP/TCP/串口等通信协议。
- **多协议支持**：
  - **网络协议**：UDP（客户端/服务器/广播/多播）、TCP（客户端/服务器）。
  - **串口通信**：跨平台串口读写，支持超时配置。
- **线程安全设计**：内置锁机制保障多线程安全，RAII 管理资源。
- **工厂模式创建实例**：通过 `CommunicationFactory` 快速创建各类通信实例。

### 3. Memory 模块
- **模块化设计**：包含 FIFO 队列、管道、循环队列、共享内存、内存缓冲区等组件。
- **线程安全**：所有组件均支持多线程安全访问。
- **灵活的数据结构**：支持固定大小块管理和动态内存缓冲区。
- **跨平台**：适配 Windows 和 POSIX 系统（Linux/macOS）。

### 4. LockManager 模块
- **RAII 原则封装**：基于 RAII (Resource Acquisition Is Initialization) 机制，自动管理互斥量的加锁与解锁，确保资源在作用域结束或异常抛出时被正确释放，极大提升并发代码的**安全性、可读性、健壮性和可维护性**。
- **多种锁类型支持**：全面支持 C++ 标准库提供的各类互斥量，包括 `std::mutex` (独占)、`std::recursive_mutex` (递归)、`std::timed_mutex` (定时独占)、`std::shared_mutex` (读写锁) 和 `std::shared_timed_mutex` (定时读写锁)。
- **死锁自动避免**：`MultiLockGuard` 类利用 C++17 的 `std::scoped_lock`，在单个原子操作中同时获取多个互斥量，内置死锁避免算法，从根本上解决多锁场景下的死锁问题。
- **条件变量支持**：`Condition` 类封装 `std::condition_variable_any`，提供强大的线程间协作和同步机制，并通过强制使用谓词 (predicate) 有效避免“虚假唤醒”。
- **C++17 标准兼容**：充分利用 `if constexpr`、`std::scoped_lock`、`std::is_same_v` 等 C++17 特性，提供更高效、更简洁的实现。

### 5. Logger 模块
- **多线程安全**：内部使用互斥锁和原子操作，确保在多线程环境下的日志写入安全。
- **跨平台支持**：主要依赖 C++ 标准库，兼容 Windows/Linux/macOS。
- **日志级别控制**：支持 DEBUG, INFO, WARNING, ERROR 四种级别，可动态调整。
- **多种输出目标**：支持控制台 (Console) 和本地文件 (File) 输出。
- **动态切换输出**：可在运行时动态切换日志的输出目标。
- **文件日志轮转**：文件输出模式下，支持按最大行数限制进行日志轮转，保留最新日志。
- **灵活配置**：通过 `LoggerConfig` 进行初始化配置。
- **清晰的日志格式**：包含时间戳、线程ID、级别、代码位置 (文件、行号、函数) 和消息。
- **便捷宏定义**：提供 `LSX_LOG_DEBUG`, `LSX_LOG_INFO` 等宏简化日志调用。

## 快速开始

### 前置条件
- C++17 或更高版本的编译器。
- 将 LIBLSX 库导入项目。

### 代码示例

#### 线程任务调度
```cpp
#include "LIBLSX/Thread/ThreadWrapper.h"
#include "LIBLSX/Thread/Scheduler.h"

using namespace LIBLSX::Thread;

// 创建线程并绑定 Lambda 任务
ThreadWrapper thread;
thread.setTask([]() {
    std::cout << "Hello from thread!" << std::endl;
});
thread.start();
thread.stop();

// 调度周期性任务
Scheduler scheduler;
scheduler.schedulePeriodic(1000, []() {
    std::cout << "Tick every 1s" << std::endl;
});
std::this_thread::sleep_for(std::chrono::seconds(3));
scheduler.shutdown();
````

#### 网络通信

```cpp
#include "LIBLSX/DataTransfer/CommunicationFactory.h"

using namespace LIBLSX;

// 创建 TCP 客户端
auto tcp = CommunicationFactory::create(CommType::TCP_CLIENT, "127.0.0.1", 8080);
if (tcp && tcp->create()) {
    std::vector<uint8_t> data = {0x01, 0x02, 0x03};
    tcp->send(data.data(), data.size());
    tcp->close();
}
```

#### 内存管理

```cpp
#include "LIBLSX/Memory/FIFO.h"
#include "LIBLSX/Memory/Pipe.h"

using namespace LIBLSX::Memory;

// 使用 FIFO 队列
FIFO<std::string> string_fifo;
string_fifo.Put("Hello");
string_fifo.Put("World");

// 使用管道
Pipe byte_pipe;
std::vector<uint8_t> send = {'H', 'e', 'l', 'l', 'o'};
byte_pipe.Write(send);
std::vector<uint8_t> received = byte_pipe.Read(5);
```

#### 锁管理 (LockManager)

```cpp
#include "LIBLSX/LockManager/MultiLockGuard.h"
#include <iostream>
#include <thread>
#include <mutex>
#include <chrono> // For std::chrono::milliseconds

using namespace LIBLSX::LockManager;

std::mutex account_m1, account_m2;
int account_balance1 = 100, account_balance2 = 200;

void transfer_funds(int amount, int& from_balance, int& to_balance, std::mutex& m_from, std::mutex& m_to) {
    std::cout << "Attempting transfer of " << amount << "..." << std::endl;
    // MultiLockGuard 原子性地获取两个互斥量，自动处理死锁
    MultiLockGuard<std::mutex, std::mutex> lock_pair(m_from, m_to);

    if (from_balance >= amount) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10)); // 模拟处理时间
        from_balance -= amount;
        to_balance += amount;
        std::cout << "Transfer successful. Bal1=" << from_balance << ", Bal2=" << to_balance << std::endl;
    } else {
        std::cout << "Transfer failed: Insufficient funds." << std::endl;
    }
}
// Example usage (within main or another function):
// std::thread t1(transfer_funds, 30, std::ref(account_balance1), std::ref(account_balance2), std::ref(account_m1), std::ref(account_m2));
// std::thread t2(transfer_funds, 80, std::ref(account_balance2), std::ref(account_balance1), std::ref(account_m2), std::ref(account_m1)); // 演示颠倒顺序仍安全
// t1.join(); t2.join();
```

#### 日志系统 (Logger)

```cpp
#include "LIBLSX/Logger/Logger.h"

using namespace LIBLSX::Logger;

int main() { // Note: Logger instance lifecycle needs management in a real app.
    LoggerConfig config;
    config.level = LogLevel::DEBUG; // 设置级别为 DEBUG
    config.mode = OutputMode::Console; // 输出到控制台

    Logger logger(config); // 创建 Logger 实例

    // 使用日志宏记录日志
    LSX_LOG_DEBUG(logger, "这是一个调试信息.");
    LSX_LOG_INFO(logger, "应用程序启动.");
    LSX_LOG_WARNING(logger, "发现一个潜在问题.");
    LSX_LOG_ERROR(logger, "发生严重错误!");

    // logger.SetLogLevel(LogLevel::INFO); // 动态改变级别示例
    // LSX_LOG_DEBUG(logger, "这条 DEBUG 日志不会被记录.");

    return 0;
}
```

## 模块文档

| 模块          | 功能说明                              | 详细文档链接                                                                       |
|---------------|---------------------------------------|------------------------------------------------------------------------------|
| **Thread** | 线程管理与任务调度                    | [Thread 模块文档](https://github.com/JinBiLianShao/liblsx/blob/master/example%2FThread%2FLIBLSX%20%E5%B7%A5%E5%85%B7%E5%BA%93%20Thread%20%E6%A8%A1%E5%9D%97%20%E4%BD%BF%E7%94%A8%E8%AF%B4%E6%98%8E%E6%96%87%E6%A1%A3.md) |
| **DataTransfer** | 网络与串口通信                      | [DataTransfer 模块文档](https://github.com/JinBiLianShao/liblsx/blob/master/example%2FDataTransfer%2FLIBLSX%20%E5%B7%A5%E5%85%B7%E5%BA%93%20DataTransfer%20%E6%A8%A1%E5%9D%97%20%E4%BD%BF%E7%94%A8%E8%AF%B4%E6%98%8E%E6%96%87%E6%A1%A3.md) |
| **Memory** | 内存管理与数据结构                    | [Memory 模块文档](https://github.com/JinBiLianShao/liblsx/blob/master/example%2FMemoryManagement%2FLIBLSX%20%E5%B7%A5%E5%85%B7%E5%BA%93%20Memory%20%E6%A8%A1%E5%9D%97%E4%BD%BF%E7%94%A8%E8%AF%B4%E6%98%8E.md) |
| **LockManager**| RAII 锁管理与并发原语               | [LockManager 模块文档](https://github.com/JinBiLianShao/liblsx/blob/master/example/LockManager/LIBLSX%20%E5%B7%A5%E5%85%B7%E5%BA%93%20LockManager%20%E6%A8%A1%E5%9D%97%E4%BD%BF%E7%94%A8%E8%AF%B4%E6%98%8E.md) |
| **Logger** | 轻量级多功能日志系统                  | [Logger 模块文档](https://github.com/JinBiLianShao/liblsx/blob/master/example/Logger/LSX_LIB%E5%B7%A5%E5%85%B7%E5%BA%93Logger%20%E6%A8%A1%E5%9D%97%E4%BD%BF%E7%94%A8%E8%AF%B4%E6%98%8E.md) |

## 项目状态与贡献

- **当前版本**：v0.2.0

### 6. Timer 模块（v0.2.0 新增）
- **单线程多定时器**：TimerWheel 用一个工作线程驱动任意数量的定时器（一次性 + 周期），支持回调内取消自己/再次调度。
- **计时工具**：Stopwatch（ms/us/秒）与 Deadline（超时判断/剩余时间）。
- **对比 Scheduler**：定时器数量多时选 TimerWheel；需要暂停/恢复控制选 Scheduler。

### 7. EventBus 模块（v0.2.0 新增）
- **类型安全发布-订阅**：事件类型即路由键，publish 同步分发给全部订阅者。
- **RAII 订阅句柄**：Subscription 析构自动退订，支持手动退订、句柄拷贝/移动。
- **回调内安全**：回调中可嵌套 publish、subscribe、unsubscribe（含退订自己）。
- **线程安全**：多线程并发发布串行分发，总线销毁自动使句柄失效。

### 8. Serialize 模块（v0.3.0 新增）
- **轻量二进制编解码**：小端固定宽度 + varint（LEB128）/zigzag + 长度前缀字符串 + TLV 字段（对齐 protobuf wire format）。
- **无异常错误处理**：读取越界/格式错误置 fail 标志，适合解析不可信网络输入。
- **协议演化**：TLV 未知字段自动跳过，新旧版本互操作。
- **header-only**，与 DataTransfer 组合构成完整的设备间通信方案。

## 构建方式（v3.0 构建系统）

### 本机编译（同时产出静态库 + 动态库）
```bash
mkdir -p build/both && cd build/both
cmake ../.. -DCOMPILER_TARGET=x86 -DTARGET_TYPE=both
make -j
# 产出：build/both/x86/static/liblsx.a  build/both/x86/shared/liblsx.so.0.1.1（带 SOVERSION 软链）
```

### 交叉编译（x86 / arm32 / arm64）
编译器选择已交给标准 toolchain 文件（`toolchains/*.cmake`），不再硬编码路径：
```bash
mkdir -p build/arm64 && cd build/arm64
cmake ../.. -DCMAKE_TOOLCHAIN_FILE=../../toolchains/arm64.cmake -DCOMPILER_TARGET=arm64 -DTARGET_TYPE=both
make -j
# 产出 arm64 架构的 liblsx.a / liblsx.so，归档在 build/arm64/arm64/ 下
```
arm32 同理，toolchain 前缀可通过 `-DARM32_TOOLCHAIN_PREFIX=...` 覆盖。

### TARGET_TYPE 选项
- `both`（默认）：同时构建 `lsx_static` 与 `lsx_shared`
- `static` / `shared`：只构建其一
- `executable`：把库源码编译为可执行文件（旧行为兼容）

### 安装
```bash
make install   # 头文件按模块装入 <prefix>/include/lsx/，库装入 <prefix>/lib/
```

## 测试（v0.1.2 新增）

库根目录 tests/ 内置深度测试套件（53 个用例，覆盖全部五大模块）：
```bash
cmake .. -DENABLE_TESTS=ON && make -j
ctest --test-dir build -R lsx_deep_tests   # 或直接运行 build/<platform>/bin/lsx_tests
```
覆盖内容：Thread 生命周期/调度/线程池并发、LockManager 全锁类型与死锁避免、
Memory 全队列/管道/共享内存（含多线程压测）、DataTransfer TCP/UDP 回环与异常路径、
Logger 级别过滤/轮转/并发写入。测试曾直接暴露并推动修复三个死锁/失效缺陷（见更新日志）。

## 更新日志

### v0.3.0（2026-11-10）
- **新增 Serialize 模块**：header-only 二进制编解码（BinaryWriter/BinaryReader），支持小端固定宽度整数、varint/zigzag、长度前缀字符串与 TLV 字段（对齐 protobuf wire format）。面向嵌入式设备间通信协议，与 DataTransfer 组合使用。
- **错误处理设计**：读取失败置 fail 标志返回零值，不抛异常——解析不可信网络输入只需最后统一检查。
- **协议演化支持**：TLV 未知字段自动跳过，新旧版本写入方/读取方可互操作。
- **Config 模块纳入测试**：INIReader 的解析、默认值、大小写不敏感、注释处理、错误报告首次获得用例覆盖。
- **新增 26 个深度测试用例**（总计 109 个），三轮稳定性全过；arm64 交叉编译零警告。

### v0.2.0（2026-11-10）
- **新增 Timer 模块**：TimerWheel（单工作线程驱动任意数量一次性/周期定时器，支持取消与回调内再调度）、Stopwatch、Deadline。对比 Scheduler 的"一任务一线程"模型，大规模定时场景资源占用大幅下降。
- **新增 EventBus 模块**：类型安全发布-订阅总线，RAII 订阅句柄（析构自动退订），回调内嵌套发布/订阅/退订均安全，多线程并发发布支持。
- **新增 30 个深度测试用例**（总计 83 个），两个新模块全部覆盖；三轮稳定性运行全过。
- **新增模块使用文档**：Timer/EventBus 各自的使用说明，含与 Scheduler 的选型对比。

### v0.1.2（2026-11-10）
- **深度测试套件**：新增 tests/ 目录（自带极简测试框架），53 个用例覆盖五大模块，接入 CTest（-DENABLE_TESTS=ON），连续多次全量运行稳定通过。
- **修复 TcpServer::acceptConnection 死锁**：持 mtx 锁期间调用 closeClientConnection() 再次加锁，std::mutex 不可重入，accept 永远执行不到。拆分出不加锁内部实现，accept/close 两条路径均修复。
- **修复 CircularQueue 首次入队死锁**：Enqueue/Dequeue 持锁调用 IsFull/IsEmpty 再次加锁。新增 IsFullUnlocked/IsEmptyUnlocked 内部检查，公开 API 行为不变。
- **修复 ThreadWrapper::restart 永远失效**：restart 内部先 stop()（清空 task_）再 start()（要求 task_ 非空），顺序矛盾导致 restart 从未成功过。现暂存并恢复任务后重新启动。
- 测试直接驱动了以上三个缺陷的发现与回归验证。

### v0.1.1（2026-11-10）
- **构建系统升级到 v3.0**：一次构建同时产出静态库与动态库（TARGET_TYPE=both，也可单独选）；交叉编译改走标准 toolchain 文件机制（内置 toolchains/x86、arm32、arm64.cmake，可覆盖前缀），不再在 CMakeLists 里硬编码编译器路径；产出物按平台归档到 build/<target>/{static,shared}/，动态库接入 SOVERSION（liblsx.so.0.1.1 软链）；源码收集改为只取 src/（旧版把 vendored 三方头也卷进 include 与 glob）；安装规则保留并经过 DESTDIR 实测。
- **修复 Scheduler::shutdown 死锁**：周期任务的退出条件从 getState() == STOPPED 改为 isStopRequested()。原实现中 STOPPED 状态要等 stop() 内部 join 完成后才写入，任务循环与 stop() 互相等待，shutdown 永久阻塞。ThreadWrapper 新增公开方法 isStopRequested() 供任务循环使用。
- **代码结构清理**：移除 17 个 .cpp 文件中误用的 #pragma once（该指令只应出现在头文件）；清理重复 include 与缩进残缺。
- **C++17 正统化**：ThreadPool::enqueue 统一使用 std::invoke_result_t，删除已废弃的 std::result_of 回退分支。
- **构建系统现代化**：project() 声明版本与描述；启用 -Wall -Wextra -Wpedantic 警告基线（警告数 17 → 0）；新增 make install 安装规则（头文件 + 库，排除 vendored 预编译产物）。
- **接口默认实现与预留参数显式化**：ICommunication 超时默认实现、SerialPort 通用超时等处的未使用参数以 (void) 标注并注明预留原因。

### v0.1.0
- 五大核心模块：Thread、DataTransfer、Memory、LockManager、Logger。
- **待开发模块**：配置解析、异步任务队列等。
- **欢迎贡献**：欢迎提交 Issue 或 PR，共同完善功能模块。请遵循项目的代码规范与协议。

## 许可证

本项目采用 **MIT 许可证**。详情请参阅 [LICENSE](https://www.google.com/search?q=LICENSE) 文件。