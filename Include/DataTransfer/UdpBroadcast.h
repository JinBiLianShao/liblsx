// LSXTransportLib: 数据传输工具库（跨平台）
// 命名空间：LSX_LIB

// ---------- File: UdpBroadcast.h ----------
#ifndef LSX_UDP_BROADCAST_H
#define LSX_UDP_BROADCAST_H
#pragma once
#include "UdpClient.h" // 继承自 UdpClient

namespace LSX_LIB::DataTransfer
{
    class UdpBroadcast : public UdpClient
    {
    public:
        explicit UdpBroadcast(uint16_t port);
        bool create() override;
    };
} // namespace LSX_LIB

#endif // LSX_UDP_BROADCAST_H
