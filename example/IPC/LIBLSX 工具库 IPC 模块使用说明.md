# LIBLSX 工具库 IPC 模块使用说明

---

## 目录

1. [模块概述](#模块概述)
2. [模块架构](#模块架构)
3. [快速上手](#快速上手)
4. [API 参考](#api-参考)
5. [超时与阻塞语义](#超时与阻塞语义)
6. [与 DataTransfer 的选型对比](#与-datatransfer-的选型对比)
7. [常见错误用法与规避](#常见错误用法与规避)

---

## 模块概述

IPC 模块基于 Unix domain socket 提供本机进程间的字节流收发（POSIX 平台）。
相比 TCP 环回（127.0.0.1），UDS 不经过网络栈、无端口占用、可用文件权限控制访问。

核心特性：

* **接口风格与 DataTransfer 一致**：create/send/receive/close，学习成本为零
* **RAII fd 管理**：析构自动关闭；服务端自动清理残留 socket 文件
* **超时可控**：accept/receive/send 均受 SO_RCVTIMEO/SO_SNDTIMEO 影响
* **单客户端模型**：与 TcpServer 一致，多客户端请自扩展线程分发

## 模块架构

```
Include/IPC/
└── IpcSocket.h        # IpcServer + IpcClient 声明
src/IPC/
└── IpcSocket.cpp      # POSIX 实现
```

命名空间：`LSX_LIB::IPC`。仅支持 POSIX 平台（嵌入式 Linux 场景）。

---

## 快速上手

### 服务端

```cpp
#include "IPC/IpcSocket.h"

LSX_LIB::IPC::IpcServer server("/tmp/lsx_demo.sock");
if (!server.create()) { /* 绑定失败：path 过长或无权限 */ }

// 阻塞等待客户端接入
if (server.acceptClient()) {
    server.send(data, len);                    // 发送
    int n = server.receive(buf, sizeof(buf));  // 接收
}

server.close();   // 关闭并删除 socket 文件
```

### 客户端

```cpp
LSX_LIB::IPC::IpcClient client("/tmp/lsx_demo.sock");
if (!client.create()) { /* 服务端未启动 */ }

client.send(data, len);
int n = client.receive(buf, sizeof(buf));
client.close();
```

---

## API 参考

### IpcServer

| 方法 | 说明 |
|------|------|
| `bool create()` | 创建监听 socket 并绑定；自动清理残留旧 socket 文件 |
| `bool acceptClient()` | 阻塞接受一个客户端（可超时） |
| `bool send(const uint8_t*, size_t)` | 向当前客户端全量发送 |
| `int receive(uint8_t*, size_t)` | 接收：>0 字节数，0=超时，-1=错误 |
| `void closeClient()` | 断开当前客户端（可继续 accept 下一个） |
| `void close()` | 关闭监听并删除 socket 文件 |
| `bool setReceiveTimeout(int ms)` | 设置超时（<=0 永久阻塞） |
| `bool isListening() / hasClient()` | 状态查询 |

### IpcClient

| 方法 | 说明 |
|------|------|
| `bool create()` | 连接服务端 path |
| `bool send / int receive` | 同服务端 |
| `bool setReceiveTimeout(int ms)` | 超时设置 |
| `void close()` | 关闭连接 |
| `bool isConnected()` | 状态查询 |

---

## 超时与阻塞语义

```cpp
server.setReceiveTimeout(100);   // 100ms
```

* timeout <= 0：永久阻塞（默认）
* accept 超时返回 false；receive 超时返回 **0**（与"对端关闭返回 0"同值，
  用 isListening/hasClient 区分场景）
* 超时设置影响 accept/receive/send 三个阻塞点

---

## 与 DataTransfer 的选型对比

| 维度 | IPC（UDS） | DataTransfer TCP 环回 |
|------|-----------|----------------------|
| 适用范围 | 仅本机进程 | 本机 + 跨网络 |
| 性能 | 不经网络栈，更快 | 有协议栈开销 |
| 访问控制 | 文件权限 | 需应用层鉴权 |
| 端口占用 | 无（path 即地址） | 需要端口管理 |
| 跨平台 | 仅 POSIX | 全平台 |

**经验法则**：同机进程通信用 IPC；跨设备用 DataTransfer。

---

## 常见错误用法与规避

### 1. socket path 过长

sun_path 上限约 108 字节。长路径 create 会返回 false，注意 /tmp 前缀下的可用长度。

### 2. 残留 socket 文件导致绑定失败

本模块已在 create 时自动清理残留文件；若你的下游自行实现，记得先 remove。

### 3. 接收端假设一次 recv 收到完整消息

UDS 是字节流协议，一次 receive 可能只收到半条消息。消息边界请用
Serialize 模块的长度前缀或 TLV 自行分帧。

### 4. 多客户端并发

单客户端模型。多客户端需求：accept 后把 conn 交给独立线程处理，
或使用 select/poll 分发（参考 TcpServer 的处理方式）。
