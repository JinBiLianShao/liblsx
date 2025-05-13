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

- **当前版本**：v0.1.0（开发中）
- **待开发模块**：配置解析、异步任务队列等。
- **欢迎贡献**：欢迎提交 Issue 或 PR，共同完善功能模块。请遵循项目的代码规范与协议。

## 许可证

本项目采用 **MIT 许可证**。详情请参阅 [LICENSE](https://www.google.com/search?q=LICENSE) 文件。