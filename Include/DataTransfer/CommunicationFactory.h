// LSXTransportLib: 数据传输工具库（跨平台）
// 命名空间：LSX_LIB

// ---------- File: CommunicationFactory.h ----------
#ifndef LSX_COMMUNICATION_FACTORY_H
#define LSX_COMMUNICATION_FACTORY_H
#pragma once
#include <memory> // 包含 std::unique_ptr, std::make_unique
#include <string> // 包含 std::string
#include "ICommunication.h"

namespace LSX_LIB::DataTransfer
{
    // 通信类型枚举
    enum class CommType
    {
        UDP_CLIENT, UDP_SERVER, UDP_BROADCAST, UDP_MULTICAST,
        TCP_CLIENT, TCP_SERVER,
        SERIAL
    };

    // 通信对象工厂类
    class CommunicationFactory
    {
    public:
        // 创建通信对象
        // type: 通信类型
        // address: IP 地址 (UDP/TCP 客户端/多播)
        // port: 端口 (UDP/TCP)
        // serialName: 串口名称 (Serial)
        // baudRate: 波特率 (Serial)
        // timeout_ms: 可选参数，设置发送和接收超时 (毫秒)，-1 表示阻塞，0 表示非阻塞，> 0 表示指定毫秒数
        static std::unique_ptr<ICommunication> create(CommType type,
                                                      const std::string& address = "", uint16_t port = 0,
                                                      const std::string& serialName = "", int baudRate = 0,
                                                      int timeout_ms = -1);
    };
} // namespace LSX_LIB

#endif // LSX_COMMUNICATION_FACTORY_H
