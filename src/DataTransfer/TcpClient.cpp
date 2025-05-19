

// ---------- File: TcpClient.cpp ----------
#pragma once
#include "TcpClient.h"
#include <cstring> // 包含 memset
#include <limits> // 包含 numeric_limits
#include <algorithm> // 包含 std::min

namespace LSX_LIB::DataTransfer
{
    TcpClient::TcpClient(const std::string& ip, uint16_t port)
    {
        std::memset(&serverAddr, 0, sizeof(serverAddr));
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(port);
        // 使用 inet_pton
#ifdef _WIN32
    if (inet_pton(AF_INET, ip.c_str(), &(serverAddr.sin_addr)) != 1) {
        LSX_LIB::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex);
        std::cerr << "TcpClient::TcpClient: inet_pton failed for address " << ip << ". Error: " << WSAGetLastError() << std::endl;
    }
#else
        if (inet_pton(AF_INET, ip.c_str(), &(serverAddr.sin_addr)) != 1)
        {
            LSX_LIB::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex);
            std::cerr << "TcpClient::TcpClient: inet_pton failed for address " << ip << ". Error: " << strerror(errno)
                << std::endl;
        }
#endif
    }

    bool TcpClient::create()
    {
        LSX_LIB::LockManager::LockGuard<std::mutex> lock(mtx); // 锁定互斥锁

#ifdef _WIN32
  sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (sockfd == INVALID_SOCKET) {
    LSX_LIB::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex); // 锁定错误输出
    std::cerr << "TcpClient::create: socket failed. Error: " << WSAGetLastError() << std::endl;
    return false;
  }
#else
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0)
        {
            LSX_LIB::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex); // 锁定错误输出
            std::cerr << "TcpClient::create: socket failed. Error: " << strerror(errno) << std::endl;
            return false;
        }
#endif

        if (connect(sockfd,
                    reinterpret_cast<struct sockaddr*>(&serverAddr),
                    sizeof(serverAddr)) < 0)
        {
#ifdef _WIN32
    LSX_LIB::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex); // 锁定错误输出
    std::cerr << "TcpClient::create: connect failed. Error: " << WSAGetLastError() << std::endl;
    closesocket(sockfd); // 清理套接字
    sockfd = INVALID_SOCKET;
#else
            LSX_LIB::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex); // 锁定错误输出
            std::cerr << "TcpClient::create: connect failed. Error: " << strerror(errno) << std::endl;
            ::close(sockfd); // 清理套接字
            sockfd = -1;
#endif
            return false;
        }
        return true;
    }

    bool TcpClient::send(const uint8_t* data, size_t size)
    {
        LSX_LIB::LockManager::LockGuard<std::mutex> lock(mtx); // 锁定互斥锁

        if (sockfd < 0
#ifdef _WIN32
    || sockfd == INVALID_SOCKET
#endif
        )
        {
            LSX_LIB::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex); // 锁定错误输出
            std::cerr << "TcpClient::send: Socket not created or closed." << std::endl;
            return false;
        }

        size_t totalSent = 0;
        // 将 size 转换为 int，如果 size 过大可能溢出，此处假定 size 在 int 范围内
        // send 函数一次发送的最大字节数通常是 int 的最大值
        if (size > static_cast<size_t>(std::numeric_limits<int>::max()))
        {
            std::lock_guard<std::mutex> lock_err_large(g_error_mutex);
            std::cerr << "TcpClient::send: Total data size exceeds maximum send chunk size." << std::endl;
            return false; // 数据过大，需要更高级的分块发送逻辑
        }
        int intSize = static_cast<int>(size); // 重新确保 intSize 正确


        while (totalSent < size)
        {
            size_t remaining = size - totalSent;
            int sendSize = static_cast<int>(std::min(remaining, static_cast<size_t>(std::numeric_limits<int>::max())));
            // 确保不超过 int 最大值

            int sent = ::send(sockfd, reinterpret_cast<const char*>(data + totalSent),
                              sendSize, 0);

            if (sent < 0)
            {
#ifdef _WIN32
      int err = WSAGetLastError();
      // 处理非阻塞/超时错误
      if (err == WSAEWOULDBLOCK) {
                 // 如果是非阻塞 socket 且发送缓冲区满，等待并重试
                 // 在阻塞模式下，send 会一直阻塞直到发送完成或发生错误
                 // 如果设置了发送超时，超时会返回 WSAETIMEDOUT
                 // 如果这里是阻塞模式设置了超时并返回 WSAEWOULDBLOCK，可能有问题
                 // 假设当前为阻塞模式或超时模式，WSAEWOULDBLOCK 不应频繁出现
                 continue; // 理论上不应该发生，除非是非阻塞 socket
            }
      if (err == WSAETIMEDOUT) {
        LSX_LIB::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex); // 锁定错误输出
        std::cerr << "TcpClient::send: Timed out." << std::endl;
        return false; // 发送超时
      }
      {
                LSX_LIB::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex); // 锁定错误输出
        std::cerr << "TcpClient::send: send failed. Error: " << err << std::endl;
            }
#else
                // 处理非阻塞/超时错误
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    // 如果是非阻塞 socket 且发送缓冲区满，等待并重试
                    // 在阻塞模式或超时模式下，不应频繁出现
                    continue; // 理论上不应该发生
                }
                {
                    LSX_LIB::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex); // 锁定错误输出
                    std::cerr << "TcpClient::send: send failed. Error: " << strerror(errno) << std::endl;
                }
#endif
                return false; // 发生错误
            }
            if (sent == 0)
            {
                // 发送时连接被对端关闭 (比接收时少见)
                LSX_LIB::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex); // 锁定错误输出
                std::cerr << "TcpClient::send: Connection closed by peer during send." << std::endl;
                return false;
            }
            totalSent += sent;
        }
        return true; // 所有数据发送完毕
    }

    int TcpClient::receive(uint8_t* buffer, size_t size)
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
            std::cerr << "TcpClient::receive: Socket not created or closed." << std::endl;
            return -1; // 指示错误
        }

        // TCP 接收读取当前可用的数据，最多读取 size 字节到缓冲区。
        // 它不保证读取到一个完整的“消息”。
        // 调用者需要循环调用并实现消息分帧（如果需要的话）。
        // 返回值约定：>0 成功（读取字节数），0 连接关闭，<0 错误。
        // 将 size 转换为 int，如果 size 过大可能溢出，此处假定 size 在 int 范围内
        int intSize = static_cast<int>(size);
        if (intSize < 0 || static_cast<size_t>(intSize) != size)
        {
            LSX_LIB::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex);
            std::cerr << "TcpClient::receive: Buffer size too large for recv chunk size." << std::endl;
            return -1;
        }


        int received = ::recv(sockfd, reinterpret_cast<char*>(buffer),
                              intSize, 0);

        if (received < 0)
        {
#ifdef _WIN32
    int err = WSAGetLastError();
    // 处理超时/非阻塞错误
    if (err == WSAEWOULDBLOCK) {
            // LSX_LIB::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex);
      // std::cerr << "TcpClient::receive: recv would block." << std::endl; // 非阻塞模式下当前无数据
            return 0; // 将无可读数据视为读取 0 字节
        }
    if (err == WSAETIMEDOUT) {
      // LSX_LIB::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex);
      // std::cerr << "TcpClient::receive: Timed out." << std::endl; // 超时可能不是错误，取决于使用场景
      return 0; // 将超时视为读取 0 字节
    }
    {
            LSX_LIB::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex); // 锁定错误输出
      std::cerr << "TcpClient::receive: recv failed. Error: " << err << std::endl;
        }
#else
            // 处理超时/非阻塞错误
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                // LSX_LIB::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex);
                // std::cerr << "TcpClient::receive: recv would block/EAGAIN." << std::endl; // 非阻塞模式下当前无数据
                return 0; // 将无可读数据视为读取 0 字节
            }
            {
                LSX_LIB::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex); // 锁定错误输出
                std::cerr << "TcpClient::receive: recv failed. Error: " << strerror(errno) << std::endl;
            }
#endif
            return -1; // 指示错误
        }
        // received == 0 表示连接被对端优雅关闭。
        return received; // 返回读取到的字节数 (>= 0)
    }

    void TcpClient::close()
    {
        LSX_LIB::LockManager::LockGuard<std::mutex> lock(mtx); // 锁定互斥锁

        if (sockfd >= 0
#ifdef _WIN32
    || sockfd != INVALID_SOCKET
#endif
        )
        {
            // 先关闭发送和接收 (可选，但好的实践)
#ifdef _WIN32
    shutdown(sockfd, SD_BOTH); // 忽略错误，因为连接可能已经断开
    closesocket(sockfd);
    sockfd = INVALID_SOCKET; // 设置句柄为无效状态
#else
            shutdown(sockfd, SHUT_RDWR); // 忽略错误，因为连接可能已经断开
            ::close(sockfd);
            sockfd = -1; // 设置句柄为无效状态
#endif
            // WSACleanup 由 WinsockManager 处理
            // {
            //     std::lock_guard<std::mutex> lock_out(g_error_mutex); // 锁定输出
            //     std::cout << "TcpClient: Socket closed." << std::endl;
            // }
        }
    }

    TcpClient::~TcpClient()
    {
        close(); // 确保在析构时调用 close
    }

    bool TcpClient::setSendTimeout(int timeout_ms)
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
    std::cerr << "TcpClient::setSendTimeout failed. Error: " << WSAGetLastError() << std::endl;
    return false;
  }
#else
        struct timeval tv;
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        if (setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0)
        {
            LSX_LIB::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex); // 锁定错误输出
            std::cerr << "TcpClient::setSendTimeout failed. Error: " << strerror(errno) << std::endl;
            return false;
        }
#endif
        return true;
    }

    bool TcpClient::setReceiveTimeout(int timeout_ms)
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
    std::cerr << "TcpClient::setReceiveTimeout failed. Error: " << WSAGetLastError() << std::endl;
    return false;
  }
#else
        struct timeval tv;
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
        {
            LSX_LIB::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex); // 锁定错误输出
            std::cerr << "TcpClient::setReceiveTimeout failed. Error: " << strerror(errno) << std::endl;
            return false;
        }
#endif
        return true;
    }
} // namespace LSX_LIB
