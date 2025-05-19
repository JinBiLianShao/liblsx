

// ---------- File: UdpMulticast.cpp ----------
#pragma once
#include "UdpMulticast.h"
#include <cstring>
#include <limits>
#include <algorithm>

namespace LSX_LIB::DataTransfer
{
    UdpMulticast::UdpMulticast(const std::string& mcastIp, uint16_t port)
        : groupIp(mcastIp)
    {
        std::memset(&groupAddr, 0, sizeof(groupAddr));
        groupAddr.sin_family = AF_INET;
        groupAddr.sin_port = htons(port);
        // 使用 inet_pton
#ifdef _WIN32
    if (inet_pton(AF_INET, mcastIp.c_str(), &(groupAddr.sin_addr)) != 1) {
        LSX_LIB::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex);
        std::cerr << "UdpMulticast::UdpMulticast: inet_pton failed for address " << mcastIp << ". Error: " << WSAGetLastError() << std::endl;
    }
#else
        if (inet_pton(AF_INET, mcastIp.c_str(), &(groupAddr.sin_addr)) != 1)
        {
            LSX_LIB::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex);
            std::cerr << "UdpMulticast::UdpMulticast: inet_pton failed for address " << mcastIp << ". Error: " <<
                strerror(errno) << std::endl;
        }
#endif
    }

    bool UdpMulticast::create()
    {
        LSX_LIB::LockManager::LockGuard<std::mutex> lock(mtx); // 锁定互斥锁

#ifdef _WIN32
  sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (sockfd == INVALID_SOCKET) {
    LSX_LIB::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex); // 锁定错误输出
    std::cerr << "UdpMulticast::create: socket failed. Error: " << WSAGetLastError() << std::endl;
    return false;
  }
#else
        sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        if (sockfd < 0)
        {
            LSX_LIB::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex); // 锁定错误输出
            std::cerr << "UdpMulticast::create: socket failed. Error: " << strerror(errno) << std::endl;
            return false;
        }
#endif

        // 允许重用多播地址/端口
        int reuse = 1;
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse)) < 0)
        {
#ifdef _WIN32
    LSX_LIB::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex); // 锁定错误输出
    std::cerr << "UdpMulticast::create: setsockopt(SO_REUSEADDR) failed. Error: " << WSAGetLastError() << std::endl;
#else
            LSX_LIB::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex); // 锁定错误输出
            std::cerr << "UdpMulticast::create: setsockopt(SO_REUSEADDR) failed. Error: " << strerror(errno) <<
                std::endl;
#endif
            // 不是关键错误，但报告
        }
#ifdef _WIN32
  // 需要绑定到端口才能接收多播消息
  struct sockaddr_in localBindAddr;
  std::memset(&localBindAddr, 0, sizeof(localBindAddr));
  localBindAddr.sin_family = AF_INET;
  localBindAddr.sin_port = groupAddr.sin_port; // 绑定到多播端口
  localBindAddr.sin_addr.s_addr = htonl(INADDR_ANY); // 绑定到任何本地接口

  if (bind(sockfd, reinterpret_cast<struct sockaddr*>(&localBindAddr), sizeof(localBindAddr)) < 0) {
    LSX_LIB::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex); // 锁定错误输出
    std::cerr << "UdpMulticast::create: bind failed. Error: " << WSAGetLastError() << std::endl;
    close(); // 清理资源 (close 已加锁)
    return false;
  }
#else
        // POSIX 系统通常需要绑定到多播端口才能接收
        struct sockaddr_in localBindAddr;
        std::memset(&localBindAddr, 0, sizeof(localBindAddr));
        localBindAddr.sin_family = AF_INET;
        localBindAddr.sin_port = groupAddr.sin_port; // 绑定到多播端口
        localBindAddr.sin_addr.s_addr = htonl(INADDR_ANY); // 绑定到任何本地接口

        if (bind(sockfd, reinterpret_cast<struct sockaddr*>(&localBindAddr), sizeof(localBindAddr)) < 0)
        {
            LSX_LIB::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex); // 锁定错误输出
            std::cerr << "UdpMulticast::create: bind failed. Error: " << strerror(errno) << std::endl;
            close(); // 清理资源 (close 已加锁)
            return false;
        }
#endif


        // 加入多播组
#ifdef _WIN32
  struct ip_mreq mreq;
  mreq.imr_multiaddr.s_addr = groupAddr.sin_addr.s_addr;
  mreq.imr_interface.s_addr = htonl(INADDR_ANY); // 在任何接口上加入
  if (setsockopt(sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP,
    reinterpret_cast<const char*>(&mreq), sizeof(mreq)) == SOCKET_ERROR) {
    LSX_LIB::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex); // 锁定错误输出
    std::cerr << "UdpMulticast::create: setsockopt(IP_ADD_MEMBERSHIP) failed. Error: " << WSAGetLastError() << std::endl;
    close(); // 清理资源 (close 已加锁)
    return false;
  }
#else
        struct ip_mreq mreq;
        mreq.imr_multiaddr.s_addr = groupAddr.sin_addr.s_addr;
        mreq.imr_interface.s_addr = htonl(INADDR_ANY); // 在任何接口上加入
        if (setsockopt(sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                       &mreq, sizeof(mreq)) < 0)
        {
            LSX_LIB::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex); // 锁定错误输出
            std::cerr << "UdpMulticast::create: setsockopt(IP_ADD_MEMBERSHIP) failed. Error: " << strerror(errno) <<
                std::endl;
            close(); // 清理资源 (close 已加锁)
            return false;
        }
#endif

        return true;
    }

    bool UdpMulticast::send(const uint8_t* data, size_t size)
    {
        LSX_LIB::LockManager::LockGuard<std::mutex> lock(mtx); // 锁定互斥锁

        if (sockfd < 0
#ifdef _WIN32
    || sockfd == INVALID_SOCKET
#endif
        )
        {
            LSX_LIB::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex); // 锁定错误输出
            std::cerr << "UdpMulticast::send: Socket not created or closed." << std::endl;
            return false;
        }

        // 将 size 转换为 int，如果 size 过大可能溢出，此处假定 size 在 int 范围内
        int intSize = static_cast<int>(size);
        if (intSize < 0 || static_cast<size_t>(intSize) != size)
        {
            LSX_LIB::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex);
            std::cerr << "UdpMulticast::send: Data size too large for sendto." << std::endl;
            return false;
        }


        // 发送数据报到多播组地址
        size_t totalSent = 0;
        while (totalSent < size)
        {
            int sent = ::sendto(sockfd, reinterpret_cast<const char*>(data + totalSent),
                                static_cast<int>(size - totalSent), 0,
                                reinterpret_cast<struct sockaddr*>(&groupAddr), sizeof(groupAddr));

            if (sent < 0)
            {
#ifdef _WIN32
      LSX_LIB::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex); // 锁定错误输出
      std::cerr << "UdpMulticast::send: sendto failed. Error: " << WSAGetLastError() << std::endl;
#else
                LSX_LIB::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex); // 锁定错误输出
                std::cerr << "UdpMulticast::send: sendto failed. Error: " << strerror(errno) << std::endl;
#endif
                return false;
            }
            if (sent == 0 && intSize > 0)
            {
                // 请求发送但实际发送0字节，这对于 sendto 来说通常不应该发生，除非是非阻塞 socket 且发送缓冲区满
                LSX_LIB::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex);
                std::cerr << "UdpMulticast::send: Warning - sendto wrote 0 bytes (" << sent << "/" << intSize <<
                    "), check non-blocking mode or error." << std::endl;
                return false; // 视为失败
            }
            totalSent += sent;
        }
        return totalSent == size; // 对于 UDP 数据报，通常一次发送成功
    }

    int UdpMulticast::receive(uint8_t* buffer, size_t size)
    {
        // 修改了函数签名
        LSX_LIB::LockManager::LockGuard<std::mutex> lock(mtx); // 锁定互斥锁

        if (sockfd < 0
#ifdef _WIN32
    || sockfd == INVALID_SOCKET
#endif
        )
        {
            LSX_LIB::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex); // 锁定错误输出
            std::cerr << "UdpMulticast::receive: Socket not created or closed." << std::endl;
            return -1; // 指示错误
        }

        // 将 size 转换为 int，如果 size 过大可能溢出，此处假定 size 在 int 范围内
        int intSize = static_cast<int>(size);
        if (intSize < 0 || static_cast<size_t>(intSize) != size)
        {
            LSX_LIB::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex);
            std::cerr << "UdpMulticast::receive: Buffer size too large for recvfrom chunk size." << std::endl;
            return -1;
        }

        // 从多播组接收一个数据报
        int received = ::recvfrom(sockfd, reinterpret_cast<char*>(buffer),
                                  intSize, 0, nullptr, nullptr); // 丢弃发送者信息

        if (received < 0)
        {
#ifdef _WIN32
    int err = WSAGetLastError();
    // 处理超时/非阻塞情况
    if (err == WSAETIMEDOUT) {
      // LSX_LIB::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex);
      // std::cerr << "UdpMulticast::receive: recvfrom timed out." << std::endl; // 超时可能不是错误
      return 0; // 将超时视为读取 0 字节
    }
    if (err == WSAEWOULDBLOCK) {
            // LSX_LIB::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex);
      // std::cerr << "UdpMulticast::receive: recvfrom would block." << std::endl; // 非阻塞模式下当前无数据
            return 0; // 将无可读数据视为读取 0 字节
        }
        {
            LSX_LIB::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex); // 锁定错误输出
      std::cerr << "UdpMulticast::receive: recvfrom failed. Error: " << err << std::endl;
        }
#else
            // 处理超时/非阻塞情况
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                // LSX_LIB::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex);
                // std::cerr << "UdpMulticast::receive: recvfrom would block/EAGAIN." << std::endl; // 非阻塞模式下当前无数据
                return 0; // 将无可读数据视为读取 0 字节
            }
            {
                LSX_LIB::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex); // 锁定错误输出
                std::cerr << "UdpMulticast::receive: recvfrom failed. Error: " << strerror(errno) << std::endl;
            }
#endif
            return -1; // 指示错误
        }

        return received; // 返回读取到的字节数 (>= 0)
    }

    void UdpMulticast::close()
    {
        LSX_LIB::LockManager::LockGuard<std::mutex> lock(mtx); // 锁定互斥锁

        // 在关闭套接字前离开多播组 (可选，但好的实践)
        if (sockfd >= 0
#ifdef _WIN32
    || sockfd != INVALID_SOCKET
#endif
        )
        {
#ifdef _WIN32
    struct ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = groupAddr.sin_addr.s_addr;
    mreq.imr_interface.s_addr = htonl(INADDR_ANY); // 在任何接口上离开
    setsockopt(sockfd, IPPROTO_IP, IP_DROP_MEMBERSHIP,
      reinterpret_cast<const char*>(&mreq), sizeof(mreq)); // 清理时忽略错误
    closesocket(sockfd);
    sockfd = INVALID_SOCKET; // 设置句柄为无效状态
#else
            struct ip_mreq mreq;
            mreq.imr_multiaddr.s_addr = groupAddr.sin_addr.s_addr;
            mreq.imr_interface.s_addr = htonl(INADDR_ANY); // 在任何接口上离开
            setsockopt(sockfd, IPPROTO_IP, IP_DROP_MEMBERSHIP, &mreq, sizeof(mreq)); // 清理时忽略错误
            ::close(sockfd);
            sockfd = -1; // 设置句柄为无效状态
#endif
            // WSACleanup 由 WinsockManager 处理
            // {
            //     std::lock_guard<std::mutex> lock_out(g_error_mutex); // 锁定输出
            //     std::cout << "UdpMulticast: Socket closed." << std::endl;
            // }
        }
    }

    UdpMulticast::~UdpMulticast()
    {
        close(); // 确保在析构时调用 close
    }

    bool UdpMulticast::setSendTimeout(int timeout_ms)
    {
        LSX_LIB::LockManager::LockGuard<std::mutex> lock(mtx); // 锁定互斥锁

        if (sockfd < 0
#ifdef _WIN32
    || sockfd == INVALID_SOCKET
#endif
        )
            return false;

#ifdef _WIN32
  DWORD timeout = timeout_ms;
  if (setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout)) == SOCKET_ERROR) {
    LSX_LIB::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex); // 锁定错误输出
    std::cerr << "UdpMulticast::setSendTimeout failed. Error: " << WSAGetLastError() << std::endl;
    return false;
  }
#else
        struct timeval tv;
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        if (setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0)
        {
            LSX_LIB::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex); // 锁定错误输出
            std::cerr << "UdpMulticast::setSendTimeout failed. Error: " << strerror(errno) << std::endl;
            return false;
        }
#endif
        return true;
    }

    bool UdpMulticast::setReceiveTimeout(int timeout_ms)
    {
        LSX_LIB::LockManager::LockGuard<std::mutex> lock(mtx); // 锁定互斥锁

        if (sockfd < 0
#ifdef _WIN32
    || sockfd == INVALID_SOCKET
#endif
        )
            return false;

#ifdef _WIN32
  DWORD timeout = timeout_ms;
  if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout)) == SOCKET_ERROR) {
    LSX_LIB::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex); // 锁定错误输出
    std::cerr << "UdpMulticast::setReceiveTimeout failed. Error: " << WSAGetLastError() << std::endl;
    return false;
  }
#else
        struct timeval tv;
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
        {
            LSX_LIB::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex); // 锁定错误输出
            std::cerr << "UdpMulticast::setReceiveTimeout failed. Error: " << strerror(errno) << std::endl;
            return false;
        }
#endif
        return true;
    }
} // namespace LSX_LIB
