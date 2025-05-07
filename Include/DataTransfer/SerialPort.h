// LSXTransportLib: 数据传输工具库（跨平台）
// 命名空间：LSX_LIB

// ---------- File: SerialPort.h ----------
#ifndef LSX_SERIAL_PORT_H
#define LSX_SERIAL_PORT_H
#pragma once
#include "ICommunication.h"
#include <string> // 包含 std::string
#ifdef _WIN32
   #include <windows.h> // 包含 Windows API 头文件
#else // POSIX 系统 (Linux, macOS, etc.)
#include <termios.h> // For termios struct and functions
#include <fcntl.h> // For file control options like O_RDWR, O_NOCTTY
#include <unistd.h> // For close, read, write
#include <errno.h> // For errno
#include <string.h> // For strerror
#endif
#include <iostream> // For std::cerr
#include <mutex> // 包含 std::mutex
#include "GlobalErrorMutex.h" // 包含全局错误锁头文件


namespace LSX_LIB::DataTransfer
{
    class SerialPort : public ICommunication
    {
    public:
        SerialPort(const std::string& portName, int baudRate);
        bool create() override;
        bool send(const uint8_t* data, size_t size) override; // 健壮的发送循环
        int receive(uint8_t* buffer, size_t size) override; // 修改返回类型，读取可用数据
        void close() override;
        ~SerialPort();

        // 串口通常有不同的超时机制 (VTIME, VMIN, Windows COMMTIMEOUTS)。
        // 这些 ICommunication 超时可能映射到它们，也可能不完全支持。
        bool setSendTimeout(int timeout_ms) override;
        bool setReceiveTimeout(int timeout_ms) override;

    private: // SerialPort 没有被其他类继承，所以保持 private 即可
        std::string portName;
        int baudRate;

#ifdef _WIN32
          HANDLE hComm = INVALID_HANDLE_VALUE; // 初始化句柄为无效状态
#else
        int fd = -1; // 初始化句柄为无效状态
        struct termios oldtio; // 保存原始设置
#endif

        std::mutex mtx; // 用于保护成员变量和串口操作的互斥锁

        // 辅助函数，获取平台特定的波特率常量
        int getPlatformBaudRate(int baudRate);
    };
} // namespace LSX_LIB

#endif // LSX_SERIAL_PORT_H
