// LSXTransportLib: 数据传输工具库（跨平台）
// 命名空间：LSX_LIB

// ---------- File: UdpClient.cpp ----------
#include "UdpClient.h"
#include <cstring> // 包含 memset
#include <limits> // 包含 numeric_limits
#include <algorithm> // 包含 std::min

namespace LSX_LIB::DataTransfer
{
    UdpClient::UdpClient(const std::string& ip, uint16_t port)
    {
        std::memset(&serverAddr, 0, sizeof(serverAddr));
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(port);
        // 使用 inet_pton 获取更健壮的地址转换和潜在的 IPv6 支持 (尽管结构是 IPv4)
#ifdef _WIN32
    if (inet_pton(AF_INET, ip.c_str(), &(serverAddr.sin_addr)) != 1) {
        std::lock_guard<std::mutex> lock(g_error_mutex);
        std::cerr << "UdpClient::UdpClient: inet_pton failed for address " << ip << ". Error: " << WSAGetLastError() << std::endl;
    }
#else
        if (inet_pton(AF_INET, ip.c_str(), &(serverAddr.sin_addr)) != 1)
        {
            std::lock_guard<std::mutex> lock(g_error_mutex);
            std::cerr << "UdpClient::UdpClient: inet_pton failed for address " << ip << ". Error: " << strerror(errno)
                << std::endl;
        }
#endif
    }

    bool UdpClient::create()
    {
        LIBLSX::LockManager::LockGuard<std::mutex> lock(mtx); // 锁定互斥锁

#ifdef _WIN32
  sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (sockfd == INVALID_SOCKET) {
    LIBLSX::LockManager::LockGuard<std::mutex> lock(g_error_mutex); // 锁定错误输出
    std::cerr << "UdpClient::create: socket failed. Error: " << WSAGetLastError() << std::endl;
    return false;
  }
#else
        sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        if (sockfd < 0)
        {
            LIBLSX::LockManager::LockGuard<std::mutex> lock(g_error_mutex); // 锁定错误输出
            std::cerr << "UdpClient::create: socket failed. Error: " << strerror(errno) << std::endl;
            return false;
        }
#endif
        return true;
    }

    bool UdpClient::send(const uint8_t* data, size_t size)
    {
        LIBLSX::LockManager::LockGuard<std::mutex> lock(mtx); // 锁定互斥锁

        if (sockfd < 0
#ifdef _WIN32
    || sockfd == INVALID_SOCKET
#endif
        )
        {
            LIBLSX::LockManager::LockGuard<std::mutex> lock(g_error_mutex); // 锁定错误输出
            std::cerr << "UdpClient::send: Socket not created or closed." << std::endl;
            return false;
        }

        // UDP 是数据报协议，sendto 通常会尝试一次发送整个数据报（如果小于MTU）。
        // 循环在这里不是强制的，但对于保证发送 size 字节的逻辑完整性有用。
        // 对于大于 MTU 的数据，用户需要自行处理分片。
        size_t totalSent = 0;
        // 将 size 转换为 int，如果 size 过大可能溢出，此处假定 size 在 int 范围内
        // sendto 函数一次发送的最大字节数通常是 int 的最大值
        if (size > static_cast<size_t>(std::numeric_limits<int>::max()))
        {
            std::lock_guard<std::mutex> lock_err_large(g_error_mutex);
            std::cerr << "UdpClient::send: Total data size exceeds maximum sendto chunk size." << std::endl;
            return false; // 数据过大，需要更高级的分块发送逻辑
        }
        int intSize = static_cast<int>(size); // 重新确保 intSize 正确


        while (totalSent < size)
        {
            size_t remaining = size - totalSent;
            int sendSize = static_cast<int>(std::min(remaining, static_cast<size_t>(std::numeric_limits<int>::max())));
            // 确保不超过 int 最大值

            int sent = ::sendto(sockfd, reinterpret_cast<const char*>(data + totalSent),
                                sendSize, 0,
                                reinterpret_cast<struct sockaddr*>(&serverAddr),
                                sizeof(serverAddr));

            if (sent < 0)
            {
#ifdef _WIN32
      int err = WSAGetLastError();
      // 如果是非阻塞 socket，需要检查 WSAEWOULDBLOCK
      {
                LIBLSX::LockManager::LockGuard<std::mutex> lock(g_error_mutex); // 锁定错误输出
        std::cerr << "UdpClient::send: sendto failed. Error: " << err << std::endl;
            }
#else
                // 如果是非阻塞 socket，需要检查 EAGAIN 或 EWOULDBLOCK
                {
                    LIBLSX::LockManager::LockGuard<std::mutex> lock(g_error_mutex); // 锁定错误输出
                    std::cerr << "UdpClient::send: sendto failed. Error: " << strerror(errno) << std::endl;
                }
#endif
                return false; // 发生错误
            }
            if (sent == 0 && sendSize > 0)
            {
                // 请求发送但实际发送0字节，这对于 sendto 来说通常不应该发生，除非是非阻塞 socket 且发送缓冲区满
                LIBLSX::LockManager::LockGuard<std::mutex> lock(g_error_mutex);
                std::cerr << "UdpClient::send: Warning - sendto wrote 0 bytes (" << sent << "/" << sendSize <<
                    "), check non-blocking mode or error." << std::endl;
                // 这里可以选择重试或视为失败
                return false; // 视为失败
            }
            totalSent += sent;
        }
        return true; // 对于 UDP 数据报，通常一次发送成功
    }

    int UdpClient::receive(uint8_t* buffer, size_t size)
    {
        // 修改了函数签名
        LIBLSX::LockManager::LockGuard<std::mutex> lock(mtx); // 锁定互斥锁

        if (sockfd < 0
#ifdef _WIN32
    || sockfd == INVALID_SOCKET
#endif
        )
        {
            LIBLSX::LockManager::LockGuard<std::mutex> lock(g_error_mutex); // 锁定错误输出
            std::cerr << "UdpClient::receive: Socket not created or closed." << std::endl;
            return -1; // 指示错误
        }

        // UDP 接收读取一个数据报，最多读取 size 字节到缓冲区。
        // 如果数据报大于 size，多余的部分将被丢弃。
        // 将 size 转换为 int，如果 size 过大可能溢出，此处假定 size 在 int 范围内
        int intSize = static_cast<int>(size);
        if (intSize < 0 || static_cast<size_t>(intSize) != size)
        {
            LIBLSX::LockManager::LockGuard<std::mutex> lock(g_error_mutex);
            std::cerr << "UdpClient::receive: Buffer size too large for recvfrom chunk size." << std::endl;
            return -1;
        }

        // 返回值约定：>0 成功（读取字节数），0 超时/无数据，<0 错误。
        int received = ::recvfrom(sockfd, reinterpret_cast<char*>(buffer),
                                  intSize, 0, nullptr, nullptr); // 丢弃发送者信息

        if (received < 0)
        {
#ifdef _WIN32
    int err = WSAGetLastError();
    // 处理超时/非阻塞情况
    if (err == WSAETIMEDOUT) {
      // LIBLSX::LockManager::LockGuard<std::mutex> lock(g_error_mutex);
      // std::cerr << "UdpClient::receive: recvfrom timed out." << std::endl; // 超时可能不是错误，取决于使用场景
      return 0; // 将超时视为读取 0 字节
    }
    if (err == WSAEWOULDBLOCK) {
             // LIBLSX::LockManager::LockGuard<std::mutex> lock(g_error_mutex);
      // std::cerr << "UdpClient::receive: recvfrom would block." << std::endl; // 非阻塞模式下当前无数据
             return 0; // 将无可读数据视为读取 0 字节
        }
        {
            LIBLSX::LockManager::LockGuard<std::mutex> lock(g_error_mutex); // 锁定错误输出
      std::cerr << "UdpClient::receive: recvfrom failed. Error: " << err << std::endl;
        }
#else
            // 处理超时/非阻塞情况
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                // LIBLSX::LockManager::LockGuard<std::mutex> lock(g_error_mutex);
                // std::cerr << "UdpClient::receive: recvfrom would block/EAGAIN." << std::endl; // 非阻塞模式下当前无数据
                return 0; // 将无可读数据视为读取 0 字节
            }
            {
                LIBLSX::LockManager::LockGuard<std::mutex> lock(g_error_mutex); // 锁定错误输出
                std::cerr << "UdpClient::receive: recvfrom failed. Error: " << strerror(errno) << std::endl;
            }
#endif
            return -1; // 指示错误
        }

        return received; // 返回读取到的字节数 (>= 0)
    }

    void UdpClient::close()
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
            //     std::cout << "UdpClient: Socket closed." << std::endl;
            // }
        }
    }

    UdpClient::~UdpClient()
    {
        close(); // 确保在析构时调用 close
    }

    bool UdpClient::setSendTimeout(int timeout_ms)
    {
        LIBLSX::LockManager::LockGuard<std::mutex> lock(mtx); // 锁定互斥锁

        if (sockfd < 0
#ifdef _WIN32
    || sockfd == INVALID_SOCKET
#endif
        )
            return false; // 如果 socket 无效，无法设置超时

#ifdef _WIN32
  DWORD timeout = timeout_ms;
  if (setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout)) == SOCKET_ERROR) {
    LIBLSX::LockManager::LockGuard<std::mutex> lock(g_error_mutex); // 锁定错误输出
    std::cerr << "UdpClient::setSendTimeout failed. Error: " << WSAGetLastError() << std::endl;
    return false;
  }
#else
        struct timeval tv;
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        if (setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0)
        {
            LIBLSX::LockManager::LockGuard<std::mutex> lock(g_error_mutex); // 锁定错误输出
            std::cerr << "UdpClient::setSendTimeout failed. Error: " << strerror(errno) << std::endl;
            return false;
        }
#endif
        return true;
    }

    bool UdpClient::setReceiveTimeout(int timeout_ms)
    {
        LIBLSX::LockManager::LockGuard<std::mutex> lock(mtx); // 锁定互斥锁

        if (sockfd < 0
#ifdef _WIN32
    || sockfd == INVALID_SOCKET
#endif
        )
            return false; // 如果 socket 无效，无法设置超时

#ifdef _WIN32
  DWORD timeout = timeout_ms;
  if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout)) == SOCKET_ERROR) {
    LIBLSX::LockManager::LockGuard<std::mutex> lock(g_error_mutex); // 锁定错误输出
    std::cerr << "UdpClient::setReceiveTimeout failed. Error: " << WSAGetLastError() << std::endl;
    return false;
  }
#else
        struct timeval tv;
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
        {
            LIBLSX::LockManager::LockGuard<std::mutex> lock(g_error_mutex); // 锁定错误输出
            std::cerr << "UdpClient::setReceiveTimeout failed. Error: " << strerror(errno) << std::endl;
            return false;
        }
#endif
        return true;
    }
} // namespace LSX_LIB
