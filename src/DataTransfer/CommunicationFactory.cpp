
// ---------- File: CommunicationFactory.cpp ----------
#pragma once
#include "CommunicationFactory.h"
#include "UdpClient.h"
#include "UdpServer.h"
#include "UdpBroadcast.h"
#include "UdpMulticast.h"
#include "TcpClient.h"
#include "TcpServer.h"
#include "SerialPort.h"

// 包含 WinsockManager 头文件，以确保其静态实例被创建和链接
#include "WinsockManager.h"

#include <iostream> // For std::cerr and std::endl


namespace LSX_LIB::DataTransfer
{
    std::unique_ptr<ICommunication> CommunicationFactory::create(
        CommType type, const std::string& address, uint16_t port,
        const std::string& serialName, int baudRate, int timeout_ms)
    {
        std::unique_ptr<ICommunication> comm = nullptr; // 初始化为空指针

        switch (type)
        {
        case CommType::UDP_CLIENT:
            comm = std::make_unique<UdpClient>(address, port);
            break;
        case CommType::UDP_SERVER:
            comm = std::make_unique<UdpServer>(port);
            break;
        case CommType::UDP_BROADCAST:
            comm = std::make_unique<UdpBroadcast>(port);
            break;
        case CommType::UDP_MULTICAST:
            comm = std::make_unique<UdpMulticast>(address, port);
            break;
        case CommType::TCP_CLIENT:
            comm = std::make_unique<TcpClient>(address, port);
            break;
        case CommType::TCP_SERVER:
            comm = std::make_unique<TcpServer>(port);
            break;
        case CommType::SERIAL:
            comm = std::make_unique<SerialPort>(serialName, baudRate);
            break;
        default:
            {
                // 使用 LIBLSX::LockManager::LockGuard 保护全局错误输出
                // LockGuard 构造时锁定 g_error_mutex，离开作用域时自动解锁
                LIBLSX::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex);
                std::cerr << "CommunicationFactory::create: Unsupported CommType." << std::endl;
            }
            break;
        }

        // 如果对象成功创建且指定了超时，则设置超时
        if (comm && timeout_ms >= 0)
        {
            comm->setSendTimeout(timeout_ms);
            comm->setReceiveTimeout(timeout_ms);
        }

        return comm;
    }
} // namespace LSX_LIB
