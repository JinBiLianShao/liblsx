

// ---------- File: UdpBroadcast.cpp ----------
#pragma once
#include "UdpBroadcast.h"
#ifdef _WIN32
   #include <winsock2.h>
#else
#include <sys/types.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <sys/socket.h>
#endif
#include <cstring>
#include <iostream>
#include <mutex>
#include "GlobalErrorMutex.h"

namespace LSX_LIB::DataTransfer
{
    // 注意: 使用 255.255.255.255 有局限性 (仅限本地网络)。
    // 对于定向广播，需要根据本地接口计算广播地址。
    UdpBroadcast::UdpBroadcast(uint16_t port)
        : UdpClient("255.255.255.255", port)
    {
    }

    bool UdpBroadcast::create()
    {
        // 调用基类 create (创建 socket, 为广播 IP 设置 serverAddr)
        // 基类的 create 已经加锁，此处无需重复加锁
        bool ok = UdpClient::create();
        if (!ok)
        {
            return false;
        }

        // 基类的 create 成功后，sockfd 已经有效
        // setsockopt 调用需要保护，因为多个线程可能创建 UdpBroadcast 对象
        // 继承的 protected 成员可以直接访问
        LSX_LIB::LockManager::LockGuard<std::mutex> lock(mtx); // 锁定互斥锁 (mtx 已改为 protected)

        // 设置 SO_BROADCAST 选项
        int opt = 1;
        if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST,
                       reinterpret_cast<const char*>(&opt), sizeof(opt)) < 0)
        {
#ifdef _WIN32
                LSX_LIB::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex); // 锁定错误输出
                std::cerr << "UdpBroadcast::create: setsockopt(SO_BROADCAST) failed. Error: " << WSAGetLastError() << std::endl;
#else
            LSX_LIB::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex); // 锁定错误输出
            std::cerr << "UdpBroadcast::create: setsockopt(SO_BROADCAST) failed. Error: " << strerror(errno) <<
                std::endl;
#endif
            close(); // 清理套接字 (close 方法已加锁)
            return false;
        }
        return true;
    }

    // 继承 send, receive, close, destructor, set timeouts 来自 UdpClient
    // 这些方法已在 UdpClient 中添加 Mutex 保护并修改 sockfd, mtx 为 protected
} // namespace LSX_LIB
