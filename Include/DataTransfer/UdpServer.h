/**
 * @file UdpServer.h
 * @brief 数据传输工具库 - UDP 服务器实现类
 * @details 定义了 LSX_LIB::DataTransfer 命名空间下的 UdpServer 类，
 * 它是 ICommunication 接口的一个具体实现，用于进行跨平台的 UDP 服务器通信。
 * 该类封装了 Windows 和 POSIX (Linux/macOS) 系统下 UDP socket 服务器操作的底层细节，
 * 提供统一的接口进行 socket 的创建、绑定端口、数据接收和发送。
 * 支持设置发送和接收超时，并在析构时自动关闭 socket。
 * **注意：** 典型的 UDP 服务器通过 `recvfrom` 接收数据并获取客户端地址，然后使用 `sendto` 向该地址发送响应。
 * 此类实现的 `send` 方法不接受目标地址，因此其发送功能受限，通常只能用于向预设的或最后接收到数据的客户端发送。
 * @author 连思鑫（liansixin）
 * @date 2025-4-8
 * @version 1.0
 *
 * ### 核心功能
 * - **跨平台支持**: 封装了 Windows Winsock2 和 POSIX socket API。
 * - **ICommunication 接口实现**: 遵循通用的通信接口，可与其他通信类型互换使用。
 * - **端口绑定**: 绑定到指定的 UDP 端口，准备接收数据。
 * - **数据接收**: 提供阻塞或超时模式的数据接收功能，通常通过 `recvfrom` 获取数据和发送方地址。
 * - **数据发送 (受限)**: 提供了 `send` 方法，但由于不接受目标地址，其在典型的 UDP 服务器场景下功能受限。
 * - **超时设置**: 支持设置发送和接收操作的超时时间（映射到平台特定的 socket 选项）。
 * - **资源管理**: 使用 RAII 模式管理 socket 句柄，确保在对象生命周期结束时正确关闭 socket。
 * - **线程安全**: 使用互斥锁保护内部状态和对底层 socket 句柄的访问，支持多线程环境下的安全使用。
 *
 * ### 使用示例
 *
 * @code
 * #include "UdpServer.h"
 * #include <iostream>
 * #include <vector>
 * #include <thread>
 * #include <atomic> // 用于优雅关闭
 * #include <csignal> // 用于信号处理
 *
 * // 全局原子标志，用于信号处理
 * std::atomic<bool> g_shutdown_request{false};
 *
 * void signal_handler(int signum) {
 * if (signum == SIGINT || signum == SIGTERM) {
 * std::cout << "\n收到信号 " << signum << ", 请求关闭..." << std::endl;
 * g_shutdown_request.store(true);
 * }
 * }
 *
 * int main() {
 * // 设置信号处理函数
 * signal(SIGINT, signal_handler);
 * signal(SIGTERM, signal_handler);
 *
 * uint16_t listen_port = 8080; // 监听端口
 *
 * try {
 * // 创建 UdpServer 对象
 * LSX_LIB::DataTransfer::UdpServer server(listen_port);
 *
 * // 创建并绑定 socket
 * if (!server.create()) {
 * std::cerr << "创建 UDP 服务器 socket 或绑定端口失败!" << std::endl;
 * return 1;
 * }
 * std::cout << "UDP 服务器 socket 创建成功，正在监听端口 " << listen_port << std::endl;
 *
 * // 设置接收超时为 100 毫秒，以便可以在主循环中检查关闭请求
 * if (server.setReceiveTimeout(100)) {
 * std::cout << "接收超时设置为 100ms 成功。" << std::endl;
 * } else {
 * std::cerr << "设置接收超时失败或不支持。" << std::endl;
 * }
 *
 * std::vector<uint8_t> recv_buffer(256);
 * std::cout << "服务器正在运行... 按 Ctrl+C 或发送 SIGTERM 信号来停止服务器..." << std::endl;
 *
 * // 主循环，接收数据直到收到关闭请求
 * while (!g_shutdown_request.load()) {
 * int received_bytes = server.receive(recv_buffer.data(), recv_buffer.size());
 * if (received_bytes > 0) {
 * // 接收到数据，通常在这里处理数据，并可能使用 sendto 回复发送方
 * // 注意：使用此类的 send 方法发送数据将受限，因为它不接受目标地址。
 * // 如果需要回复特定的客户端，需要修改 receive 方法来获取客户端地址，
 * // 并使用 sendto 或提供一个接受目标地址的 send 方法。
 *
 * std::cout << "收到 " << received_bytes << " 字节数据: ";
 * // 打印接收到的数据 (仅打印可显示字符)
 * for (int i = 0; i < received_bytes; ++i) {
 * std::cout << (isprint(static_cast<unsigned char>(recv_buffer[i])) ? static_cast<char>(recv_buffer[i]) : '.');
 * }
 * std::cout << std::endl;
 *
 * // 示例：使用受限的 send 方法发送回数据 (如果需要回复，通常需要知道发送方地址)
 * // if (server.send(recv_buffer.data(), received_bytes)) {
 * //     std::cout << "使用受限 send 方法发送数据成功。" << std::endl;
 * // } else {
 * //     std::cerr << "使用受限 send 方法发送数据失败。" << std::endl;
 * // }
 *
 * } else if (received_bytes == 0) {
 * // 接收超时，没有数据，继续循环
 * // std::cout << "接收超时，继续等待..." << std::endl;
 * } else { // received_bytes < 0
 * // 错误
 * if (errno == EWOULDBLOCK || errno == EAGAIN) {
 * // 非阻塞或超时，没有数据，继续循环
 * } else {
 * std::cerr << "数据接收错误，错误码: " << received_bytes << std::endl;
 * // 发生错误，考虑退出循环或进行错误恢复
 * // break;
 * }
 * }
 * // 小睡片刻，避免 CPU 占用过高 (如果 receive 是非阻塞的且没有超时)
 * // std::this_thread::sleep_for(std::chrono::milliseconds(1));
 * }
 *
 * // 收到信号后，对象析构时会自动关闭 socket
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
 * - **无连接**: UDP 是无连接协议。服务器接收到的每个数据报都是独立的。
 * - **发送功能受限**: 由于 `send` 方法不接受目标地址，此类的发送功能在典型的 UDP 服务器场景下非常有限。要向特定的客户端发送响应，需要修改 `receive` 方法以获取客户端地址，并使用 `sendto` 或提供一个接受目标地址的发送方法。
 * - **错误处理**: 错误信息通常会打印到 `std::cerr`，并可能通过 `receive` 方法的负返回值指示。
 * - **线程安全**: `UdpServer` 类内部通过互斥锁保护了成员变量和 socket 操作的线程安全，可以在多线程环境中使用同一个 `UdpServer` 对象进行发送和接收（但需要注意同步逻辑，特别是如何处理接收到的数据和发送响应）。
 */

// LSXTransportLib: 数据传输工具库（跨平台）
// 命名空间：LSX_LIB

#ifndef LSX_UDP_SERVER_H
#define LSX_UDP_SERVER_H
#pragma once
#include "ICommunication.h" // 包含通信接口基类
#include <cstdint> // 包含 uint16_t

// 根据平台包含相应的头文件
#ifdef _WIN32
   #include <winsock2.h> // 包含 Winsock 主要头文件，用于 socket, bind, sendto, recvfrom, closesocket 等
   #include <ws2tcpip.h> // For socklen_t on some versions, inet_pton (地址转换和类型定义)
   #pragma comment(lib, "ws2_32.lib") // 链接 Winsock 库
#else // POSIX 系统 (Linux, macOS, etc.)
#include <sys/socket.h> // 包含 socket 相关函数和常量 (如 AF_INET, SOCK_DGRAM, IPPROTO_UDP, shutdown, SHUT_BOTH, socklen_t)
#include <netinet/in.h> // For sockaddr_in, INADDR_ANY, htons, htonl (地址结构和字节序转换)
#include <arpa/inet.h> // For inet_pton (IP地址字符串转二进制) - 虽然服务器通常绑定 INADDR_ANY，但包含以备用
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
         * @brief UDP 服务器实现类。
         * 实现了 ICommunication 接口，用于在 Windows 和 POSIX 系统上创建一个监听特定端口的 UDP 服务器。
         * 封装了平台特定的 socket 服务器操作细节，包括绑定端口和接收数据报。
         * **重要提示：** 此类的 `send` 方法功能受限，不接受目标地址。典型的 UDP 服务器需要知道发送方地址才能回复。
         */
        class UdpServer : public ICommunication
        {
        public:
            /**
             * @brief 构造函数。
             * 初始化 UdpServer 对象，设置服务器要监听的端口。
             * 注意：构造函数不创建 socket 或绑定端口，需要调用 create() 方法。
             *
             * @param port 服务器要监听的端口号。
             */
            explicit UdpServer(uint16_t port);

            /**
             * @brief 创建并绑定 UDP socket。
             * 创建 UDP socket，并将其绑定到指定的端口（通常是所有可用网络接口）。
             *
             * @return 如果成功创建并绑定 socket，返回 true；否则返回 false。
             */
            bool create() override;

            /**
             * @brief 发送数据报 (功能受限)。
             * 将指定缓冲区中的数据作为一个数据报发送出去。
             * **重要提示：** 此方法不接受目标地址。在典型的 UDP 服务器场景中，需要知道接收到数据的客户端地址才能回复。
             * 此方法可能只能用于向预设的或最后接收到数据的客户端地址发送（如果内部有机制记录）。
             * 通常，UDP 服务器会使用 `sendto` 函数，该函数需要目标地址和端口。
             *
             * @param data 指向要发送数据的缓冲区。
             * @param size 要发送的数据的字节数。
             * @return 如果成功将数据放入发送队列，返回 true；否则返回 false。
             */
            // 注意: UdpServer 的 send 方法通常需要一个目标地址，ICommunication 的 send 接口不支持这个。
            // 典型的服务器会从 receive 获取地址，并使用 sendto 发送。
            bool send(const uint8_t* data, size_t size) override; // 功能受限

            /**
             * @brief 接收数据报。
             * 从 socket 接收一个数据报到指定的缓冲区。
             * 此方法会尝试读取可用数据，其行为受设置的接收超时影响。
             * 在典型的 UDP 服务器实现中，此方法内部会使用 `recvfrom` 来同时获取数据和发送方的地址信息。
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
            ~UdpServer();

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

        private: // UdpServer 没有被其他类继承，所以保持 private 即可
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
             * @brief 服务器本地地址结构体。
             * 存储服务器监听的 IP 地址和端口信息。
             */
            struct sockaddr_in localAddr;

            /**
             * @brief 互斥锁。
             * 用于保护 UdpServer 对象的成员变量和对底层 socket 句柄的访问，确保线程安全。
             */
            std::mutex mtx; // 用于保护成员变量和socket操作的互斥锁
        };
    } // namespace DataTransfer
} // namespace LSX_LIB

#endif // LSX_UDP_SERVER_H
