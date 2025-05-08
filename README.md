# LIBLSX - 轻量级 C++ 跨平台工具库

[![MIT License](https://img.shields.io/badge/license-MIT-blue)](LICENSE)
![C++11](https://img.shields.io/badge/C++-11-blue.svg)

LIBLSX 是一个轻量级、跨平台的 C++ 工具库，旨在为开发者提供高效且易用的基础组件。当前已开发完成 **线程管理（Thread）**、**数据通信（DataTransfer）** 和 **内存管理（Memory）** 三大核心模块。

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

## 快速开始

### 前置条件
- C++11 或更高版本的编译器。
- 将 LIBLSX 库导入项目。

### 代码示例

#### 线程任务调度
```cpp
#include "LSX_LIB/Thread/ThreadWrapper.h"
#include "LSX_LIB/Thread/Scheduler.h"

using namespace LSX_LIB::Thread;

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
```

#### 网络通信
```cpp
#include "LSX_LIB/DataTransfer/CommunicationFactory.h"

using namespace LSX_LIB;

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
#include "LSX_LIB/Memory/FIFO.h"
#include "LSX_LIB/Memory/Pipe.h"

using namespace LSX_LIB::Memory;

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

## 模块文档

| 模块          | 功能说明                              | 详细文档链接                                                                       |
|---------------|---------------------------------------|------------------------------------------------------------------------------|
| **Thread**    | 线程管理与任务调度                    | [Thread 模块文档](https://github.com/JinBiLianShao/liblsx/blob/master/example%2FThread%2FLIBLSX%20%E5%B7%A5%E5%85%B7%E5%BA%93%20Thread%20%E6%A8%A1%E5%9D%97%20%E4%BD%BF%E7%94%A8%E8%AF%B4%E6%98%8E%E6%96%87%E6%A1%A3.md)                            |
| **DataTransfer** | 网络与串口通信                      | [DataTransfer 模块文档](https://github.com/JinBiLianShao/liblsx/blob/master/example%2FDataTransfer%2FLIBLSX%20%E5%B7%A5%E5%85%B7%E5%BA%93%20DataTransfer%20%E6%A8%A1%E5%9D%97%20%E4%BD%BF%E7%94%A8%E8%AF%B4%E6%98%8E%E6%96%87%E6%A1%A3.md) |
| **Memory**    | 内存管理与数据结构                    | [Memory 模块文档](https://github.com/JinBiLianShao/liblsx/blob/master/example%2FMemoryManagement%2FLIBLSX%20%E5%B7%A5%E5%85%B7%E5%BA%93%20Memory%20%E6%A8%A1%E5%9D%97%E4%BD%BF%E7%94%A8%E8%AF%B4%E6%98%8E.md)                           |

## 项目状态与贡献

- **当前版本**：v0.1.0（开发中）
- **待开发模块**：日志系统、配置解析、异步任务队列等。
- **欢迎贡献**：欢迎提交 Issue 或 PR，共同完善功能模块。请遵循项目的代码规范与协议。

## 许可证

本项目采用 **MIT 许可证**。详情请参阅 [LICENSE](LICENSE) 文件。