/**
 * @file SerialPort.h
 * @brief 数据传输工具库 - 串口通信实现类
 * @details 定义了 LSX_LIB::DataTransfer 命名空间下的 SerialPort 类，
 * 它是 ICommunication 接口的一个具体实现，用于进行跨平台的串口通信。
 * 该类封装了 Windows 和 POSIX (Linux/macOS) 系统下串口操作的底层细节，
 * 提供统一的接口进行串口的打开、配置、数据发送和接收。
 * 支持设置发送和接收超时，并在析构时自动关闭串口。
 * @author 连思鑫（liansixin）
 * @date 2025-4-8
 * @version 1.0
 *
 * ### 核心功能
 * - **跨平台支持**: 封装了 Windows 和 POSIX 系统下的串口 API。
 * - **ICommunication 接口实现**: 遵循通用的通信接口，可与其他通信类型互换使用。
 * - **串口配置**: 在创建时指定串口名称和波特率。
 * - **数据发送/接收**: 提供阻塞或超时模式的数据发送和接收功能。
 * - **超时设置**: 支持设置发送和接收操作的超时时间（映射到平台特定的超时机制）。
 * - **资源管理**: 使用 RAII 模式管理串口句柄，确保在对象生命周期结束时正确关闭串口。
 * - **线程安全**: 使用互斥锁保护内部状态和对底层串口句柄的访问，支持多线程环境下的安全使用。
 *
 * ### 使用示例
 *
 * @code
 * #include "SerialPort.h"
 * #include <iostream>
 * #include <vector>
 * #include <thread>
 *
 * // 示例：使用 SerialPort 发送和接收数据
 * int main() {
 * // 根据平台选择串口名称
 * #ifdef _WIN32
 * const std::string port_name = "COM1";
 * #else
 * const std::string port_name = "/dev/ttyS0"; // 或 "/dev/ttyUSB0" 等
 * #endif
 *
 * int baud_rate = 9600;
 *
 * try {
 * // 创建 SerialPort 对象
 * LSX_LIB::DataTransfer::SerialPort serial(port_name, baud_rate);
 *
 * // 打开串口
 * if (!serial.create()) {
 * std::cerr << "打开串口 " << port_name << " 失败!" << std::endl;
 * return 1;
 * }
 * std::cout << "串口 " << port_name << " 打开成功，波特率 " << baud_rate << std::endl;
 *
 * // 设置接收超时为 100 毫秒
 * if (serial.setReceiveTimeout(100)) {
 * std::cout << "接收超时设置为 100ms 成功。" << std::endl;
 * } else {
 * std::cerr << "设置接收超时失败或不支持。" << std::endl;
 * }
 *
 * // 发送数据
 * const uint8_t send_data[] = "Hello from Serial!";
 * if (serial.send(send_data, sizeof(send_data) - 1)) { // -1 排除 null 终止符
 * std::cout << "数据发送成功: " << send_data << std::endl;
 * } else {
 * std::cerr << "数据发送失败。" << std::endl;
 * }
 *
 * // 接收数据
 * std::vector<uint8_t> recv_buffer(256);
 * std::cout << "等待接收数据 (最多等待 100ms)..." << std::endl;
 * int received_bytes = serial.receive(recv_buffer.data(), recv_buffer.size());
 * if (received_bytes > 0) {
 * std::cout << "收到 " << received_bytes << " 字节数据: ";
 * for (int i = 0; i < received_bytes; ++i) {
 * std::cout << static_cast<char>(recv_buffer[i]);
 * }
 * std::cout << std::endl;
 * } else if (received_bytes == 0) {
 * std::cout << "接收超时，没有收到数据。" << std::endl;
 * } else {
 * std::cerr << "数据接收错误，错误码: " << received_bytes << std::endl;
 * }
 *
 * // 串口对象在 main 函数结束时自动关闭
 *
 * } catch (const std::exception& e) {
 * std::cerr << "发生异常: " << e.what() << std::endl;
 * return 1;
 * }
 *
 * std::cout << "程序结束。" << std::endl;
 * return 0;
 * }
 * @endcode
 *
 * ### 注意事项
 * - **平台依赖**: 底层实现依赖于具体的操作系统 API (Windows 或 POSIX)。
 * - **串口名称**: 串口名称格式在不同平台下不同 (例如 Windows 的 "COMx"，Linux 的 "/dev/ttySx" 或 "/dev/ttyUSBx")。
 * - **波特率支持**: 并非所有波特率都受所有系统或硬件支持。`getPlatformBaudRate` 函数会尝试映射，但可能需要根据实际情况调整。
 * - **超时设置**: `setSendTimeout` 和 `setReceiveTimeout` 的实现会尝试映射到平台特定的超时机制 (如 Windows 的 COMMTIMEOUTS 或 POSIX 的 VTIME/VMIN)，但其行为可能与网络套接字的超时略有不同。
 * - **错误处理**: 错误信息通常会打印到 `std::cerr`，并可能通过 `receive` 方法的负返回值指示。
 * - **线程安全**: `SerialPort` 类内部通过互斥锁保证了成员变量和串口操作的线程安全，可以在多线程环境中使用同一个 `SerialPort` 对象进行发送和接收（但需要注意同步逻辑）。
 */

// LSXTransportLib: 数据传输工具库（跨平台）
// 命名空间：LSX_LIB

#ifndef LSX_SERIAL_PORT_H
#define LSX_SERIAL_PORT_H
#pragma once
#include "ICommunication.h" // 包含通信接口基类
#include <string> // 包含 std::string

// 根据平台包含相应的头文件
#ifdef _WIN32
   #include <windows.h> // 包含 Windows API 头文件，用于串口操作 (HANDLE, COMMTIMEOUTS等)
#else // POSIX 系统 (Linux, macOS, etc.)
#include <termios.h> // For termios struct and functions (tcgetattr, tcsetattr, cfsetispeed, cfsetospeed等)
#include <fcntl.h> // For file control options like O_RDWR, O_NOCTTY (open函数标志)
#include <unistd.h> // For close, read, write (串口文件描述符操作)
#include <errno.h> // For errno (错误码)
#include <string.h> // For strerror (错误码转字符串)
#endif

#include <iostream> // For std::cerr (错误输出)
#include <mutex> // 包含 std::mutex (用于线程同步)
#include "GlobalErrorMutex.h" // 包含全局错误锁头文件 (用于同步错误输出)


/**
 * @brief LSX 库的根命名空间。
 */
namespace LSX_LIB
{
    /**
     * @brief 数据传输相关的命名空间。
     * 包含各种通信类和工具。
     */
    namespace DataTransfer
    {
        /**
         * @brief 串口通信实现类。
         * 实现了 ICommunication 接口，用于在 Windows 和 POSIX 系统上进行串口通信。
         * 封装了平台特定的串口操作细节。
         */
        class SerialPort : public ICommunication
        {
        public:
            /**
             * @brief 构造函数。
             * 初始化 SerialPort 对象，设置串口名称和波特率。
             * 注意：构造函数不打开串口，需要调用 create() 方法来建立连接。
             *
             * @param portName 串口名称 (例如 Windows 上的 "COM1"，Linux 上的 "/dev/ttyS0")。
             * @param baudRate 波特率 (例如 9600, 115200)。
             */
            SerialPort(const std::string& portName, int baudRate);

            /**
             * @brief 打开并配置串口。
             * 根据构造函数中指定的串口名称和波特率打开串口设备，并进行基本的配置。
             *
             * @return 如果成功打开并配置串口，返回 true；否则返回 false。
             */
            bool create() override;

            /**
             * @brief 发送数据。
             * 将指定缓冲区中的数据通过串口发送出去。
             * 此方法会尝试发送所有指定字节，可能阻塞直到数据发送完成或发生错误/超时。
             *
             * @param data 指向要发送数据的缓冲区。
             * @param size 要发送的数据的字节数。
             * @return 如果成功发送所有数据，返回 true；否则返回 false。
             */
            bool send(const uint8_t* data, size_t size) override; // 健壮的发送循环

            /**
             * @brief 接收数据。
             * 从串口接收数据到指定的缓冲区。
             * 此方法会尝试读取可用数据，其行为受设置的接收超时影响。
             *
             * @param buffer 指向用于存储接收到数据的缓冲区。
             * @param size 接收缓冲区的最大大小（字节）。
             * @return 返回接收到的字节数。
             * - > 0: 成功接收到的字节数。
             * - 0: 在非阻塞模式或设置了超时的情况下，当前没有可用的数据。
             * - < 0: 发生错误。
             */
            int receive(uint8_t* buffer, size_t size) override; // 修改返回类型，读取可用数据

            /**
             * @brief 关闭串口连接并释放资源。
             * 关闭打开的串口句柄。多次调用是安全的。
             */
            void close() override;

            /**
             * @brief 析构函数。
             * 在对象销毁时自动关闭串口，释放资源。
             */
            ~SerialPort();

            /**
             * @brief 设置发送操作的超时时间。
             * 尝试将 ICommunication 接口的超时设置映射到平台特定的串口发送超时机制。
             *
             * @param timeout_ms 超时时间，单位为毫秒。
             * - -1: 设置为阻塞模式。
             * - 0: 设置为非阻塞模式。
             * - > 0: 设置指定的超时时间。
             * @return 如果成功设置超时，返回 true；如果该平台或实现不支持，返回 false。
             */
            bool setSendTimeout(int timeout_ms) override;

            /**
             * @brief 设置接收操作的超时时间。
             * 尝试将 ICommunication 接口的超时设置映射到平台特定的串口接收超时机制。
             *
             * @param timeout_ms 超时时间，单位为毫秒。
             * - -1: 设置为阻塞模式。
             * - 0: 设置为非阻塞模式。
             * - > 0: 设置指定的超时时间。
             * @return 如果成功设置超时，返回 true；如果该平台或实现不支持，返回 false。
             */
            bool setReceiveTimeout(int timeout_ms) override;

        private: // SerialPort 没有被其他类继承，所以保持 private 即可
            /**
             * @brief 串口名称。
             * 存储构造函数中指定的串口名称字符串。
             */
            std::string portName;
            /**
             * @brief 波特率。
             * 存储构造函数中指定的波特率数值。
             */
            int baudRate;

#ifdef _WIN32
            /**
             * @brief Windows 串口句柄。
             * 在 Windows 系统下，用于标识打开的串口设备。
             * 初始化为 INVALID_HANDLE_VALUE。
             */
            HANDLE hComm = INVALID_HANDLE_VALUE; // 初始化句柄为无效状态
#else
            /**
             * @brief POSIX 串口文件描述符。
             * 在 POSIX 系统下，用于标识打开的串口设备。
             * 初始化为 -1。
             */
            int fd = -1; // 初始化句柄为无效状态
            /**
             * @brief POSIX 原始终端设置。
             * 在 POSIX 系统下，用于保存串口打开前的原始终端设置，以便在关闭时恢复。
             */
            struct termios oldtio; // 保存原始设置
#endif

            /**
             * @brief 互斥锁。
             * 用于保护 SerialPort 对象的成员变量和对底层串口句柄的访问，确保线程安全。
             */
            std::mutex mtx; // 用于保护成员变量和串口操作的互斥锁

            /**
             * @brief 辅助函数，获取平台特定的波特率常量。
             * 将整数波特率值映射到 Windows 或 POSIX 系统对应的波特率常量。
             *
             * @param baudRate 整数波特率值。
             * @return 平台特定的波特率常量。如果不支持该波特率，可能返回一个默认值或错误指示。
             */
            int getPlatformBaudRate(int baudRate);
        };
    } // namespace DataTransfer
} // namespace LSX_LIB

#endif // LSX_SERIAL_PORT_H
