// LSXTransportLib: 数据传输工具库（跨平台）
// 命名空间：LSX_LIB

// ---------- File: UdpServer.h ----------
#ifndef LSX_UDP_SERVER_H
#define LSX_UDP_SERVER_H
#pragma once
#include "ICommunication.h"
#include <cstdint> // 包含 uint16_t
#ifdef _WIN32
   #include <winsock2.h> // 包含 Winsock 主要头文件
   #include <ws2tcpip.h> // For socklen_t on some versions, inet_pton
#else // POSIX 系统 (Linux, macOS, etc.)
#include <sys/socket.h> // 包含 socket 相关函数和常量 (如 AF_INET, SOCK_DGRAM, IPPROTO_UDP, shutdown, SHUT_BOTH, socklen_t)
#include <netinet/in.h> // For sockaddr_in, INADDR_ANY, htons, htonl
#include <arpa/inet.h> // For inet_pton
#include <unistd.h> // For close
#include <errno.h> // For errno
#include <string.h> // For strerror
#include <sys/time.h> // For timeval (POSIX socket timeout option)
#endif
#include <iostream> // For std::cerr
#include <mutex> // 包含 std::mutex
#include "GlobalErrorMutex.h" // 包含全局错误锁头文件


namespace LSX_LIB::DataTransfer
{
    class UdpServer : public ICommunication
    {
    public:
        explicit UdpServer(uint16_t port);
        bool create() override;
        // 注意: UdpServer 的 send 方法通常需要一个目标地址，ICommunication 的 send 接口不支持这个。
        // 典型的服务器会从 receive 获取地址，并使用 sendto 发送。
        bool send(const uint8_t* data, size_t size) override; // 功能受限
        int receive(uint8_t* buffer, size_t size) override; // 修改返回类型
        void close() override;
        ~UdpServer();

        bool setSendTimeout(int timeout_ms) override;
        bool setReceiveTimeout(int timeout_ms) override;

    private:
#ifdef _WIN32
          SOCKET sockfd = INVALID_SOCKET; // 初始化句柄为无效状态
#else
        int sockfd = -1; // 初始化句柄为无效状态
#endif
        struct sockaddr_in localAddr;

        std::mutex mtx; // 用于保护成员变量和socket操作的互斥锁
    };
} // namespace LSX_LIB

#endif // LSX_UDP_SERVER_H
