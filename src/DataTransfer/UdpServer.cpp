

// ---------- File: UdpServer.cpp ----------
#pragma once
#include "UdpServer.h"
#include <cstring>
#include <limits>

namespace LSX_LIB::DataTransfer
{
    UdpServer::UdpServer(uint16_t port)
    {
        std::memset(&localAddr, 0, sizeof(localAddr));
        localAddr.sin_family = AF_INET;
        localAddr.sin_port = htons(port);
        localAddr.sin_addr.s_addr = htonl(INADDR_ANY); // 使用 htonl for INADDR_ANY
    }

    bool UdpServer::create()
    {
        LIBLSX::LockManager::LockGuard<std::mutex> lock(mtx); // 锁定互斥锁

#ifdef _WIN32
  sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (sockfd == INVALID_SOCKET) {
    LIBLSX::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex); // 锁定错误输出
    std::cerr << "UdpServer::create: socket failed. Error: " << WSAGetLastError() << std::endl;
    return false;
  }
#else
        sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        if (sockfd < 0)
        {
            LIBLSX::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex); // 锁定错误输出
            std::cerr << "UdpServer::create: socket failed. Error: " << strerror(errno) << std::endl;
            return false;
        }
#endif
        // 允许套接字地址重用
        int opt = 1;
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt)) < 0)
        {
#ifdef _WIN32
    LIBLSX::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex); // 锁定错误输出
    std::cerr << "UdpServer::create: setsockopt(SO_REUSEADDR) failed. Error: " << WSAGetLastError() << std::endl;
#else
            LIBLSX::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex); // 锁定错误输出
            std::cerr << "UdpServer::create: setsockopt(SO_REUSEADDR) failed. Error: " << strerror(errno) << std::endl;
#endif
            // 不是关键错误，但报告
        }

        if (bind(sockfd,
                 reinterpret_cast<struct sockaddr*>(&localAddr),
                 sizeof(localAddr)) < 0)
        {
#ifdef _WIN32
    LIBLSX::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex); // 锁定错误输出
    std::cerr << "UdpServer::create: bind failed. Error: " << WSAGetLastError() << std::endl;
    closesocket(sockfd); // 清理套接字
    sockfd = INVALID_SOCKET;
#else
            LIBLSX::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex); // 锁定错误输出
            std::cerr << "UdpServer::create: bind failed. Error: " << strerror(errno) << std::endl;
            ::close(sockfd); // 清理套接字
            sockfd = -1;
#endif
            return false;
        }
        return true;
    }

    bool UdpServer::send(const uint8_t* data, size_t size)
    {
        LIBLSX::LockManager::LockGuard<std::mutex> lock(mtx); // 锁定互斥锁
        // 注意: 这个 send 方法不适用于典型的 UDP 服务器
        // 因为 ICommunication 接口没有提供目标地址。
        // 一个真正的 UDP 服务器需要从 receive 获取接收者地址，并使用 sendto 发送。
        LIBLSX::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex); // 锁定错误输出
        std::cerr << "UdpServer::send: This method is not typically used by a UDP server."
            << " A server needs the recipient address from receive() to use sendto." << std::endl;
        return false;
    }

    int UdpServer::receive(uint8_t* buffer, size_t size)
    {
        // 修改了函数签名
        LIBLSX::LockManager::LockGuard<std::mutex> lock(mtx); // 锁定互斥锁

        if (sockfd < 0
#ifdef _WIN32
    || sockfd == INVALID_SOCKET
#endif
        )
        {
            LIBLSX::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex); // 锁定错误输出
            std::cerr << "UdpServer::receive: Socket not created or closed." << std::endl;
            return -1; // 指示错误
        }

        struct sockaddr_in clientAddr;
        socklen_t addrLen = sizeof(clientAddr);
        // 接收一个数据报，并保存发送者的地址
        // 将 size 转换为 int，如果 size 过大可能溢出，此处假定 size 在 int 范围内
        int intSize = static_cast<int>(size);
        if (intSize < 0 || static_cast<size_t>(intSize) != size)
        {
            LIBLSX::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex);
            std::cerr << "UdpServer::receive: Buffer size too large for recvfrom chunk size." << std::endl;
            return -1;
        }

        int received = ::recvfrom(sockfd, reinterpret_cast<char*>(buffer),
                                  intSize, 0,
                                  reinterpret_cast<struct sockaddr*>(&clientAddr), &addrLen);

        if (received < 0)
        {
#ifdef _WIN32
    int err = WSAGetLastError();
    // 处理超时/非阻塞情况
    if (err == WSAETIMEDOUT) {
      // LIBLSX::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex);
      // std::cerr << "UdpServer::receive: recvfrom timed out." << std::endl; // 超时可能不是错误
      return 0; // 将超时视为读取 0 字节
    }
    if (err == WSAEWOULDBLOCK) {
            // LIBLSX::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex);
      // std::cerr << "UdpServer::receive: recvfrom would block." << std::endl; // 非阻塞模式下当前无数据
            return 0; // 将无可读数据视为读取 0 字节
        }
        {
            LIBLSX::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex); // 锁定错误输出
      std::cerr << "UdpServer::receive: recvfrom failed. Error: " << err << std::endl;
        }
#else
            // 处理超时/非阻塞情况
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                // LIBLSX::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex);
                // std::cerr << "UdpServer::receive: recvfrom would block/EAGAIN." << std::endl; // 非阻塞模式下当前无数据
                return 0; // 将无可读数据视为读取 0 字节
            }
            {
                LIBLSX::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex); // 锁定错误输出
                std::cerr << "UdpServer::receive: recvfrom failed. Error: " << strerror(errno) << std::endl;
            }
#endif
            return -1; // 指示错误
        }

        // 在一个真正的服务器中，通常需要保存 clientAddr
        // 以便之后使用 sendto 进行回复。
        // 示例: std::cout << "Received from: " << inet_ntoa(clientAddr.sin_addr) << ":" << ntohs(clientAddr.sin_port) << std::endl;

        return received; // 返回读取到的字节数
    }

    void UdpServer::close()
    {
        LIBLSX::LockManager::LockGuard<std::mutex> lock(mtx); // 锁定互斥锁

        if (sockfd >= 0
#ifdef _WIN32
    || sockfd != INVALID_SOCKET
#endif
        )
        {
#ifdef _WIN32
    closesocket(sockfd);
    sockfd = INVALID_SOCKET; // 设置句柄为无效状态
#else
            ::close(sockfd);
            sockfd = -1; // 设置句柄为无效状态
#endif
            // WSACleanup 由 WinsockManager 处理
            // {
            //     std::lock_guard<std::mutex> lock_out(g_error_mutex); // 锁定输出
            //     std::cout << "UdpServer: Socket closed." << std::endl;
            // }
        }
    }

    UdpServer::~UdpServer()
    {
        close(); // 确保在析构时调用 close
    }

    bool UdpServer::setSendTimeout(int timeout_ms)
    {
        LIBLSX::LockManager::LockGuard<std::mutex> lock(mtx); // 锁定互斥锁

        if (sockfd < 0
#ifdef _WIN32
    || sockfd == INVALID_SOCKET
#endif
        )
            return false;

#ifdef _WIN32
  DWORD timeout = timeout_ms;
  if (setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout)) == SOCKET_ERROR) {
    LIBLSX::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex); // 锁定错误输出
    std::cerr << "UdpServer::setSendTimeout failed. Error: " << WSAGetLastError() << std::endl;
    return false;
  }
#else
        struct timeval tv;
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        if (setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0)
        {
            LIBLSX::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex); // 锁定错误输出
            std::cerr << "UdpServer::setSendTimeout failed. Error: " << strerror(errno) << std::endl;
            return false;
        }
#endif
        return true;
    }

    bool UdpServer::setReceiveTimeout(int timeout_ms)
    {
        LIBLSX::LockManager::LockGuard<std::mutex> lock(mtx); // 锁定互斥锁

        if (sockfd < 0
#ifdef _WIN32
    || sockfd == INVALID_SOCKET
#endif
        )
            return false;

#ifdef _WIN32
  DWORD timeout = timeout_ms;
  if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout)) == SOCKET_ERROR) {
    LIBLSX::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex); // 锁定错误输出
    std::cerr << "UdpServer::setReceiveTimeout failed. Error: " << WSAGetLastError() << std::endl;
    return false;
  }
#else
        struct timeval tv;
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
        {
            LIBLSX::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex); // 锁定错误输出
            std::cerr << "UdpServer::setReceiveTimeout failed. Error: " << strerror(errno) << std::endl;
            return false;
        }
#endif
        return true;
    }
} // namespace LSX_LIB
