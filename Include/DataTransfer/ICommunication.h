// LSXTransportLib: 数据传输工具库（跨平台）
// 命名空间：LSX_LIB

// ---------- File: ICommunication.h ----------
#ifndef LSX_I_COMMUNICATION_H
#define LSX_I_COMMUNICATION_H
#pragma once
#include <cstdint> // 包含 uint8_t
#include <cstddef> // 包含 size_t

namespace LSX_LIB::DataTransfer
{
    // 抽象通信接口类
    class ICommunication
    {
    public:
        // 初始化或打开连接（如创建 socket 或打开串口）
        // 返回 true 表示成功， false 表示失败
        virtual bool create() = 0;

        // 发送数据
        // data: 数据缓冲区，size: 数据大小
        // 返回 true 表示成功发送所有数据， false 表示失败
        virtual bool send(const uint8_t* data, size_t size) = 0;

        // 接收数据
        // buffer: 接收缓冲区，size: 缓冲区最大大小
        // 返回值: > 0 表示接收到的字节数
        //      0 表示连接关闭 (仅 TCP) 或超时/无数据 (取决于超时配置)
        //      < 0 表示发生错误
        virtual int receive(uint8_t* buffer, size_t size) = 0;

        // 关闭连接和释放资源
        // 调用多次是安全的
        virtual void close() = 0;

        // 设置发送超时 (毫秒)
        // 并非所有通信类型都支持，不支持的实现可以返回 false
        // timeout_ms = -1: 阻塞
        // timeout_ms = 0: 非阻塞 (立即返回)
        // timeout_ms > 0: 超时时间
        virtual bool setSendTimeout(int timeout_ms) { return false; }

        // 设置接收超时 (毫秒)
        // 并非所有通信类型都支持，不支持的实现可以返回 false
        // timeout_ms = -1: 阻塞
        // timeout_ms = 0: 非阻塞 (立即返回)
        // timeout_ms > 0: 超时时间
        virtual bool setReceiveTimeout(int timeout_ms) { return false; }

        // 虚析构函数，确保派生类对象能正确销毁
        virtual ~ICommunication()
        {
        }
    };
} // namespace LSX_LIB

#endif // LSX_I_COMMUNICATION_H
