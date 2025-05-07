// LSXTransportLib: 数据传输工具库（跨平台）
// 命名空间：LSX_LIB

// ---------- File: TcpServer.h ----------
#ifndef LSX_TCP_SERVER_H
#define LSX_TCP_SERVER_H
#pragma once
#include "ICommunication.h"
#ifdef _WIN32
 #include <winsock2.h> // 包含 Winsock 主要头文件
 #include <ws2tcpip.h> // For socklen_t on some versions, inet_pton
#else // POSIX 系统 (Linux, macOS, etc.)
#include <sys/socket.h> // 包含 socket 相关函数和常量 (如 AF_INET, SOCK_STREAM, IPPROTO_TCP, shutdown, SHUT_BOTH, socklen_t)
#include <netinet/in.h> // For sockaddr_in, INADDR_ANY, htons, htonl
#include <arpa/inet.h> // For inet_pton
#include <unistd.h> // For close
#include <errno.h> // For errno
#include <string.h> // For strerror
#include <sys/time.h> // For timeval (POSIX socket timeout option)
#include <sys/select.h> // For timeval definition on some systems
#endif
#include <iostream> // For std::cerr
#include <mutex> // 包含 std::mutex
#include "GlobalErrorMutex.h" // 包含全局错误锁头文件


namespace LSX_LIB::DataTransfer
{
    // 注意: 此 TCP 服务器实现是 *单客户端*。
    // 它只监听一个连接并仅与该客户端交互。
    // 对于多客户端服务器，需要完全不同的设计（例如，使用 select/poll/epoll
    // 或线程/异步 I/O），并且 ICommunication 接口可能需要
    // 为每个连接返回新的对象。
    class TcpServer : public ICommunication
    {
    public:
        explicit TcpServer(uint16_t port);
        bool create() override; // 创建监听 socket，绑定并开始监听。不接受连接。

        // 接受一个待处理的连接。这是阻塞的（除非设置了监听 socket 为非阻塞或超时）。
        // 返回 true 成功， false 失败 (例如，如果设置了超时，则超时)
        // 注意：这个方法会修改内部的 connFd，如果多个线程同时调用此方法，只有第一个会成功接受连接。
        // 对于多线程服务器，通常在一个专门的接受线程中调用此方法。
        // 如果在调用 create 后设置了 listenFd 的接收超时，acceptConnection 会在超时后返回 false。
        bool acceptConnection();

        // 在当前已接受的连接上发送数据。
        bool send(const uint8_t* data, size_t size) override; // 健壮的发送循环

        // 在当前已接受的连接上接收数据。
        // 返回值: >0 读取字节数, 0 连接关闭, <0 错误。
        // 如果没有接受连接 (connFd 无效)，此方法将报告错误并返回 -1。
        int receive(uint8_t* buffer, size_t size) override; // 修改返回类型

        // 关闭当前已接受的连接和监听 socket。
        void close() override;

        // 仅关闭当前已接受的连接。
        // 调用多次是安全的。
        void closeClientConnection();

        ~TcpServer();

        // 注意: 超时适用于在 *已接受* 连接上的 send/receive，
        // 而不是通过 SO_RCVTIMEO 设置在监听 socket 上的 acceptConnection() 调用本身。
        // (虽然可以在 listenFd 上设置 SO_RCVTIMEO 来影响 accept，但这里 setReceiveTimeout 仅应用于 connFd)
        bool setSendTimeout(int timeout_ms) override;
        bool setReceiveTimeout(int timeout_ms) override;

    private: // TcpServer 没有被其他类继承，所以保持 private 即可
#ifdef _WIN32
  SOCKET listenFd = INVALID_SOCKET; // 初始化句柄为无效状态
  SOCKET connFd = INVALID_SOCKET; // 初始化句柄为无效状态
#else
        int listenFd = -1; // 初始化句柄为无效状态
        int connFd = -1; // 初始化句柄为无效状态
#endif
        struct sockaddr_in localAddr;

        std::mutex mtx; // 用于保护成员变量 (listenFd, connFd) 和 socket 操作的互斥锁
    };
} // namespace LSX_LIB

#endif // LSX_TCP_SERVER_H
