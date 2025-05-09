/**
 * @file UdpClient.h
 * @brief 数据传输工具库 - UDP 客户端实现类
 * @details 定义了 LSX_LIB::DataTransfer 命名空间下的 UdpClient 类，
 * 它是 ICommunication 接口的一个具体实现，用于进行跨平台的 UDP 客户端通信。
 * 该类封装了 Windows 和 POSIX (Linux/macOS) 系统下 UDP socket 客户端操作的底层细节，
 * 提供统一的接口进行 socket 的创建、数据发送和接收。
 * 支持设置发送和接收超时，并在析构时自动关闭 socket。
 * 此类通常用于向特定的目标 IP 地址和端口发送数据，并可选择性地绑定到本地端口接收数据。
 * @author 连思鑫（liansixin）
 * @date 2025-4-8
 * @version 1.0
 *
 * ### 核心功能
 * - **跨平台支持**: 封装了 Windows Winsock2 和 POSIX socket API。
 * - **ICommunication 接口实现**: 遵循通用的通信接口，可与其他通信类型互换使用。
 * - **无连接通信**: 实现 UDP 无连接数据报通信。
 * - **数据发送/接收**: 提供阻塞或超时模式的数据发送和接收功能。
 * - **超时设置**: 支持设置发送和接收操作的超时时间（映射到平台特定的 socket 选项）。
 * - **资源管理**: 使用 RAII 模式管理 socket 句柄，确保在对象生命周期结束时正确关闭 socket。
 * - **线程安全**: 使用互斥锁保护内部状态和对底层 socket 句柄的访问，支持多线程环境下的安全使用。
 * - **可继承性**: 将成员变量设置为 protected，以便派生类 (如 UdpBroadcast) 可以访问和重用。
 *
 * ### 使用示例
 *
 * @code
 * #include "UdpClient.h"
 * #include <iostream>
 * #include <vector>
 * #include <thread>
 *
 * // 示例：使用 UdpClient 向服务器发送和接收数据
 * int main() {
 * const std::string server_ip = "127.0.0.1"; // 服务器 IP 地址
 * uint16_t server_port = 8080; // 服务器端口
 *
 * try {
 * // 创建 UdpClient 对象
 * LSX_LIB::DataTransfer::UdpClient client(server_ip, server_port);
 *
 * // 创建并配置 socket
 * if (!client.create()) {
 * std::cerr << "创建 UDP 客户端 socket 失败!" << std::endl;
 * return 1;
 * }
 * std::cout << "UDP 客户端 socket 创建成功，目标 " << server_ip << ":" << server_port << std::endl;
 *
 * // 设置接收超时为 500 毫秒
 * if (client.setReceiveTimeout(500)) {
 * std::cout << "接收超时设置为 500ms 成功。" << std::endl;
 * } else {
 * std::cerr << "设置接收超时失败或不支持。" << std::endl;
 * }
 *
 * // 发送数据
 * const uint8_t send_data[] = "Hello from UDP Client!";
 * if (client.send(send_data, sizeof(send_data) - 1)) { // -1 排除 null 终止符
 * std::cout << "数据发送成功: " << send_data << std::endl;
 * } else {
 * std::cerr << "数据发送失败。" << std::endl;
 * }
 *
 * // 接收数据
 * std::vector<uint8_t> recv_buffer(256);
 * std::cout << "等待接收数据 (最多等待 500ms)..." << std::endl;
 * int received_bytes = client.receive(recv_buffer.data(), recv_buffer.size());
 * if (received_bytes > 0) {
 * std::cout << "收到 " << received_bytes << " 字节数据: ";
 * for (int i = 0; i < received_bytes; ++i) {
 * std::cout << static_cast<char>(recv_buffer[i]);
 * }
 * std::cout << std::endl;
 * } else if (received_bytes == 0) {
 * std::cout << "接收超时，没有收到数据。" << std::endl;
 * } else { // received_bytes < 0
 * std::cerr << "数据接收错误，错误码: " << received_bytes << std::endl;
 * }
 *
 * // UdpClient 对象在 main 函数结束时自动关闭 socket
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
 * - **平台依赖**: 底层实现依赖于具体的操作系统 API (Windows Winsock2 或 POSIX socket)。
 * - **无连接**: UDP 是无连接协议。每次发送都是一个独立的数据报。接收时，数据报可能乱序、重复或丢失。
 * - **绑定本地端口**: UdpClient 构造函数接受目标 IP 和端口。如果需要绑定特定的本地端口以接收数据，可能需要在 `create` 方法中进行额外的绑定操作，或者修改构造函数以接受本地端口参数。当前设计主要用于向指定目标发送。
 * - **错误处理**: 错误信息通常会打印到 `std::cerr`，并可能通过 `receive` 方法的负返回值指示。
 * - **线程安全**: `UdpClient` 类内部通过互斥锁保护了成员变量和 socket 操作的线程安全，可以在多线程环境中使用同一个 `UdpClient` 对象进行发送和接收（但需要注意同步逻辑）。
 */

// LSXTransportLib: 数据传输工具库（跨平台）
// 命名空间：LSX_LIB

#ifndef LSX_UDP_CLIENT_H
#define LSX_UDP_CLIENT_H
#pragma once
// 包含全局错误锁头文件
#include "GlobalErrorMutex.h"
// 包含 LIBLSX::LockManager::LockGuard 头文件
#include "LockGuard.h"
#include "ICommunication.h" // 包含通信接口基类
#include <string> // 包含 std::string

// 根据平台包含相应的头文件
#ifdef _WIN32
    #include <winsock2.h> // 包含 Winsock 主要头文件，用于 socket, sendto, recvfrom, closesocket 等
    #include <ws2tcpip.h> // For inet_pton, socklen_t (地址转换和类型定义)
    #pragma comment(lib, "ws2_32.lib") // 链接 Winsock 库
#else // POSIX 系统 (Linux, macOS, etc.)
#include <sys/socket.h> // 包含 socket 相关函数和常量 (如 AF_INET, SOCK_DGRAM, IPPROTO_UDP, shutdown, SHUT_BOTH)
#include <netinet/in.h> // For sockaddr_in, INADDR_ANY, htons, htonl (地址结构和字节序转换)
#include <arpa/inet.h> // For inet_pton (IP地址字符串转二进制)
#include <unistd.h> // For close (关闭文件描述符)
#include <errno.h> // For errno (错误码)
#include <string.h> // For strerror (错误码转字符串)
#include <sys/time.h> // For timeval (POSIX socket timeout option)
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
         * @brief UDP 客户端实现类。
         * 实现了 ICommunication 接口，用于在 Windows 和 POSIX 系统上进行 UDP 客户端通信。
         * 封装了平台特定的 socket 客户端操作细节。
         * 通常用于向特定的目标 IP 地址和端口发送数据报。
         */
        class UdpClient : public ICommunication
        {
        public:
            /**
             * @brief 构造函数。
             * 初始化 UdpClient 对象，设置目标服务器的 IP 地址和端口。
             * 注意：构造函数不创建 socket，需要调用 create() 方法。
             *
             * @param ip 目标服务器的 IP 地址字符串 (例如 "127.0.0.1")。
             * @param port 目标服务器的端口号。
             */
            UdpClient(const std::string& ip, uint16_t port);

            /**
             * @brief 创建并配置 UDP socket。
             * 根据构造函数中指定的目标 IP 和端口创建 UDP socket。
             * 此方法可能还会进行本地地址绑定（如果需要接收数据）。
             *
             * @return 如果成功创建并配置 socket，返回 true；否则返回 false。
             */
            bool create() override;

            /**
             * @brief 发送数据报。
             * 将指定缓冲区中的数据作为一个数据报发送到构造函数中指定的目标地址和端口。
             * UDP 是无连接的，此方法仅将数据放入发送缓冲区，不保证对方收到。
             *
             * @param data 指向要发送数据的缓冲区。
             * @param size 要发送的数据的字节数。
             * @return 如果成功将数据放入发送队列，返回 true；否则返回 false。
             */
            bool send(const uint8_t* data, size_t size) override;

            /**
             * @brief 接收数据报。
             * 从 socket 接收一个数据报到指定的缓冲区。
             * 此方法会尝试读取可用数据，其行为受设置的接收超时影响。
             *
             * @param buffer 指向用于存储接收到数据的缓冲区。
             * @param size 接收缓冲区的最大大小（字节）。
             * @return 返回接收到的字节数。
             * - > 0: 成功接收到的字节数。
             * - 0: 在非阻塞模式或设置了超时的情况下，当前没有可用的数据。
             * - < 0: 发生错误。
             */
            int receive(uint8_t* buffer, size_t size) override; // 修改返回类型

            /**
             * @brief 关闭 UDP socket 并释放资源。
             * 关闭打开的 socket 句柄。多次调用是安全的。
             */
            void close() override;

            /**
             * @brief 析构函数。
             * 在对象销毁时自动关闭 socket，释放资源。
             */
            ~UdpClient();

            /**
             * @brief 设置发送操作的超时时间。
             * 尝试将 ICommunication 接口的超时设置映射到平台特定的 socket 发送超时选项 (SO_SNDTIMEO)。
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
             * 尝试将 ICommunication 接口的超时设置映射到平台特定的 socket 接收超时选项 (SO_RCVTIMEO)。
             *
             * @param timeout_ms 超时时间，单位为毫秒。
             * - -1: 设置为阻塞模式。
             * - 0: 设置为非阻塞模式。
             * - > 0: 设置指定的超时时间。
             * @return 如果成功设置超时，返回 true；如果该平台或实现不支持，返回 false。
             */
            bool setReceiveTimeout(int timeout_ms) override;

        protected: // 更改为 protected 以便派生类 (如 UdpBroadcast) 访问
#ifdef _WIN32
            /**
             * @brief Windows socket 句柄。
             * 在 Windows 系统下，用于标识 UDP socket。
             * 初始化为 INVALID_SOCKET。
             */
            SOCKET sockfd = INVALID_SOCKET; // 初始化句柄为无效状态
#else
            /**
             * @brief POSIX socket 文件描述符。
             * 在 POSIX 系统下，用于标识 UDP socket。
             * 初始化为 -1。
             */
            int sockfd = -1; // 初始化句柄为无效状态
#endif
            /**
             * @brief 目标服务器地址结构体。
             * 存储要发送数据报的目标 IP 地址和端口信息。
             */
            struct sockaddr_in serverAddr;

            /**
             * @brief 互斥锁。
             * 用于保护 UdpClient 对象的成员变量和对底层 socket 句柄的访问，确保线程安全。
             */
            std::mutex mtx; // 用于保护成员变量和socket操作的互斥锁
        };
    } // namespace DataTransfer
} // namespace LSX_LIB

#endif // LSX_UDP_CLIENT_H
