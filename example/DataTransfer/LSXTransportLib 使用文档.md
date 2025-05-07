# LIBLSX 工具库 DataTransfer 模块 使用说明文档

---

## 目录

1. [概述](#概述)
2. [快速上手](#快速上手)
3. [模块架构](#模块架构)
4. [工厂接口：CommunicationFactory](#工厂接口communicationfactory)
5. [公共接口：ICommunication](#公共接口icommunication)
6. [各通信类型详解](#各通信类型详解)

    * [UDP 客户端（UdpClient）](#udp-客户端udpclient)
    * [UDP 服务器（UdpServer）](#udp-服务器udpserver)
    * [UDP 广播（UdpBroadcast）](#udp-广播udpbroadcast)
    * [UDP 多播（UdpMulticast）](#udp-多播udpmulticast)
    * [TCP 客户端（TcpClient）](#tcp-客户端tcpclient)
    * [TCP 服务器（TcpServer）](#tcp-服务器tcpserver)
    * [串口通信（SerialPort）](#串口通信serialport)
7. [线程安全与日志](#线程安全与日志)
8. [示例代码](#示例代码)
9. [FAQ](#faq)

---

## 概述

**DataTransfer 模块** 是 **LIBLSX** 工具库中负责网络与串口通信的跨平台组件，目标：

* **统一接口**：`ICommunication` 抽象所有通信类型
* **多协议支持**：UDP（客户端/服务器/广播/多播）、TCP（客户端/服务器）、串口
* **跨平台**：Windows + POSIX（Linux/macOS）
* **线程安全**：内部 `std::mutex` + 全局错误锁，支持多线程并发使用同一实例
* **资源管理**：RAII 管理 socket/串口句柄，自动清理

---

## 快速上手

1. **包含头文件**

   ```cpp
   #include "CommunicationFactory.h"
   #include "ICommunication.h"
   #include "GlobalErrorMutex.h"  // extern std::mutex g_error_mutex
   ```
2. **定义日志宏**

   ```cpp
   #define LOG_INFO(...)  do { std::lock_guard<std::mutex> L(LSX_LIB::g_error_mutex); std::cout << __VA_ARGS__; } while(0)
   #define LOG_ERROR(...) do { std::lock_guard<std::mutex> L(LSX_LIB::g_error_mutex); std::cerr << __VA_ARGS__; } while(0)
   ```
3. **创建 & 初始化**

   ```cpp
   using namespace LSX_LIB;
   auto comm = CommunicationFactory::create(
       CommType::TCP_CLIENT,    // 通信类型
       "192.168.1.100", 8888,   // address, port
       "", 0,                   // serialName, baudRate
       5000                     // timeout_ms
   );
   if (!comm || !comm->create()) {
       LOG_ERROR("初始化失败\n");
       return;
   }
   ```
4. **发送 / 接收**

   ```cpp
   // 发送
   std::vector<uint8_t> data = {0x01,0x02,0x03};
   if (!comm->send(data.data(), data.size())) {
       LOG_ERROR("发送失败\n");
   }
   // 接收
   std::vector<uint8_t> buf(1024);
   int n = comm->receive(buf.data(), buf.size());
   if (n > 0) {
       LOG_INFO("收到 " << n << " 字节\n");
   }
   ```
5. **关闭**

   ```cpp
   comm->close();
   ```

---

## 模块架构

```
LSX_LIB/DataTransfer
├─ ICommunication.h      // 抽象接口
├─ CommunicationFactory.h// 工厂方法
├─ UdpClient.h/.cpp
├─ UdpServer.h/.cpp
├─ UdpBroadcast.h/.cpp
├─ UdpMulticast.h/.cpp
├─ TcpClient.h/.cpp
├─ TcpServer.h/.cpp
├─ SerialPort.h/.cpp
├─ GlobalErrorMutex.h/.cpp // extern std::mutex
└─ …  
```

---

## 工厂接口：CommunicationFactory

```cpp
namespace LSX_LIB {
  enum class CommType {
    UDP_CLIENT, UDP_SERVER,
    UDP_BROADCAST, UDP_MULTICAST,
    TCP_CLIENT, TCP_SERVER,
    SERIAL
  };

  class CommunicationFactory {
  public:
    static std::unique_ptr<ICommunication>
    create(CommType type,
           const std::string& address = "",
           uint16_t port = 0,
           const std::string& serialName = "",
           int baudRate = 0,
           int timeout_ms = -1);
  };
}
```

* **参数说明**

    * `type`：通信类型
    * `address`：网络地址 / 多播组
    * `port`：网络端口
    * `serialName`：串口设备
    * `baudRate`：串口波特率
    * `timeout_ms`：发送/接收超时（`-1` 阻塞，`0` 非阻塞，`>0` 毫秒）

---

## 公共接口：ICommunication

```cpp
class ICommunication {
public:
  virtual ~ICommunication() = default;
  virtual bool create() = 0;
  virtual bool send(const uint8_t* data, size_t size) = 0;
  virtual int  receive(uint8_t* buffer, size_t size) = 0;
  virtual void close() = 0;
  virtual bool setSendTimeout(int ms) = 0;
  virtual bool setReceiveTimeout(int ms) = 0;
};
```

* **create()**：打开底层资源
* **send()**：发送所有字节
* **receive()**：返回 ≥0 字节数；0 表示超时或无数据；<0 错误
* **close()**：释放资源
* **setSend/ReceiveTimeout()**：可选实现，部分类型暂不支持

---

## 各通信类型详解

### UDP 客户端（UdpClient）

```cpp
auto udpC = CommunicationFactory::create(
    CommType::UDP_CLIENT, "192.168.1.200", 9000);
```

* **create()**

    * `socket(AF_INET, SOCK_DGRAM)`
    * 不 bind 本地（可选）
    * 记录服务器地址
* **send()** → `sendto`
* **receive()** → `recvfrom`，忽略发送者或保留（内部）
* **超时**：通过 `setsockopt(SO_RCVTIMEO/SO_SNDTIMEO)`

### UDP 服务器（UdpServer）

```cpp
auto udpS = CommunicationFactory::create(
    CommType::UDP_SERVER, "", 9000);
```

* **create()**

    * `socket(AF_INET, SOCK_DGRAM)` + `bind(port)` + `SO_REUSEADDR`
* **receive()** → `recvfrom` 并内部缓存客户端地址
* **send()** → 若无参数不可直接回复；可扩展接口获得缓存的客户端地址

### UDP 广播（UdpBroadcast）

```cpp
auto udpB = CommunicationFactory::create(
    CommType::UDP_BROADCAST, "", 9000);
```

* **基于 UdpClient**
* **create()** 后调用 `setsockopt(SO_BROADCAST)`
* **send()** → 广播地址 `255.255.255.255`

### UDP 多播（UdpMulticast）

```cpp
auto udpM = CommunicationFactory::create(
    CommType::UDP_MULTICAST, "239.0.0.1", 5000);
```

* **create()**

    * `socket` + `bind(port)` + `setsockopt(IP_ADD_MEMBERSHIP)`
* **send()** → 组播地址
* **receive()** → 组播数据

---

### TCP 客户端（TcpClient）

```cpp
auto tcpC = CommunicationFactory::create(
    CommType::TCP_CLIENT, "10.0.0.1", 8080, "", 0, 3000);
```

* **create()** → `socket()` + `connect()`
* **send()** → 循环 `send()` 保证全发
* **receive()** → `recv()`，0 表示远端关闭
* **超时** → `setsockopt(SO_RCVTIMEO/SO_SNDTIMEO)`

---

### TCP 服务器（TcpServer）

```cpp
auto tcpS = CommunicationFactory::create(
    CommType::TCP_SERVER, "", 8080);
```

* **create()** → `socket()` + `bind()` + `listen()`
* **特有方法**

  ```cpp
  bool acceptConnection();  // 阻塞等待，成功后内部保存 connFd
  void closeClientConnection();
  ```
* **send/receive** → 基于已 accept 的 `connFd`
* **注意**：仅单连接；多连接请自扩展多线程或 `select`

---

### 串口通信（SerialPort）

```cpp
auto sp = CommunicationFactory::create(
    CommType::SERIAL, "", 0, "/dev/ttyUSB0", 115200, 100);
```

* **create()**

    * Windows：`CreateFileA` + `DCB` + `COMMTIMEOUTS`
    * POSIX：`open()` + `termios(c_cflag, VTIME/VMIN)`
* **send()** → `WriteFile` / `write()` 循环全发
* **receive()** → `ReadFile` / `read()`，超时返回 0
* **close()** → `CloseHandle` / `close()` + 恢复原设置
* **超时接口**：占位，默认不生效

---

## 线程安全与日志

* **实例锁**：每个实例方法最外层 `std::lock_guard<std::mutex> mtx`
* **全局错误锁**：`extern std::mutex g_error_mutex` 保护 `std::cerr` / `std::cout`
* **建议**：使用上述 `LOG_INFO` / `LOG_ERROR` 宏

---

## 示例代码

```cpp
#include "CommunicationFactory.h"
#include "GlobalErrorMutex.h"
#include <thread>
#include <vector>

#define LOG_INFO(...)  do { std::lock_guard<std::mutex> L(LSX_LIB::g_error_mutex); std::cout << __VA_ARGS__; } while(0)
#define LOG_ERROR(...) do { std::lock_guard<std::mutex> L(LSX_LIB::g_error_mutex); std::cerr << __VA_ARGS__; } while(0)

int main() {
    using namespace LSX_LIB;

    // 创建 TCP 客户端
    auto tcp = CommunicationFactory::create(CommType::TCP_CLIENT, "127.0.0.1", 9000, "", 0, 2000);
    if (!tcp || !tcp->create()) {
        LOG_ERROR("TCP init fail\n"); return -1;
    }
    LOG_INFO("TCP connected\n");

    // 创建 UDP 多播
    auto udpM = CommunicationFactory::create(CommType::UDP_MULTICAST, "239.1.2.3", 5001);
    if (udpM && udpM->create()) LOG_INFO("Multicast ready\n");

    // 创建串口
    auto sp = CommunicationFactory::create(CommType::SERIAL, "", 0,
                                           #ifdef _WIN32 "COM4" #else "/dev/ttyS0" #endif,
                                           115200, 100);
    if (sp && sp->create()) LOG_INFO("Serial open\n");

    // 并发接收示例
    std::thread rx([&](){
        std::vector<uint8_t> buf(256);
        while (true) {
            int n = udpM->receive(buf.data(), buf.size());
            if (n>0) LOG_INFO("Mcast recv " << n << "\n");
        }
    });

    // 发送示例
    std::vector<uint8_t> pkt = {0xA5,0x5A};
    tcp->send(pkt.data(), pkt.size());
    sp->send(pkt.data(), pkt.size());

    std::this_thread::sleep_for(std::chrono::seconds(2));
    tcp->close(); udpM->close(); sp->close();
    rx.detach();
    return 0;
}
```

---

## FAQ

1. **UDP 服务器如何回复特定客户端？**

    * 本库 `send()` 不暴露客户端地址，需在 `recvfrom` 后保存地址并自行调用 `sendto`。
2. **多连接 TCP 服务器？**

    * 建议在外部循环中调用 `acceptConnection()`，或为每次 `connFd` 新建 `TcpClient` 实例处理。
3. **串口超时精度不准？**

    * POSIX `VTIME` 单位 0.1s，Windows `COMMTIMEOUTS` 精度有限。需根据需求自定义。
4. **setSend/ReceiveTimeout 总为 false？**

    * 串口超时接口仅占位，需根据注释自行实现。
5. **在多线程中安全使用？**

    * 同一实例可跨线程并发调用；不同实例互不影响；日志请使用 `LOG_*` 宏。

---
