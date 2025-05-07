// LSXTransportLib: 数据传输工具库（跨平台）
// 命名空间：LSX_LIB

// ---------- File: UdpClient.h ----------
#ifndef LSX_UDP_CLIENT_H
#define LSX_UDP_CLIENT_H
#pragma once
#include "ICommunication.h"
#include <string> // 包含 std::string
#ifdef _WIN32
    #include <winsock2.h> // 包含 Winsock 主要头文件
    #include <ws2tcpip.h> // For inet_pton, socklen_t
#else // POSIX 系统 (Linux, macOS, etc.)
#include <sys/socket.h> // 包含 socket 相关函数和常量 (如 AF_INET, SOCK_DGRAM, IPPROTO_UDP, shutdown, SHUT_BOTH)
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
    class UdpClient : public ICommunication
    {
    public:
        UdpClient(const std::string& ip, uint16_t port);
        bool create() override;
        bool send(const uint8_t* data, size_t size) override;
        int receive(uint8_t* buffer, size_t size) override; // 修改返回类型
        void close() override;
        ~UdpClient();

        bool setSendTimeout(int timeout_ms) override;
        bool setReceiveTimeout(int timeout_ms) override;

    protected: // 更改为 protected 以便派生类 (如 UdpBroadcast) 访问
#ifdef _WIN32
            SOCKET sockfd = INVALID_SOCKET; // 初始化句柄为无效状态
#else
        int sockfd = -1; // 初始化句柄为无效状态
#endif
        struct sockaddr_in serverAddr;

        std::mutex mtx; // 用于保护成员变量和socket操作的互斥锁
    };
} // namespace LSX_LIB

#endif // LSX_UDP_CLIENT_H
