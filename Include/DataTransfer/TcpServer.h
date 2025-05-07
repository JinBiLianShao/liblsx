/**
 * @file TcpServer.h
 * @brief 数据传输工具库 - TCP 单客户端服务器实现类
 * @details 定义了 LSX_LIB::DataTransfer 命名空间下的 TcpServer 类，
 * 它是 ICommunication 接口的一个具体实现，用于进行跨平台的 TCP 服务器通信。
 * **注意：这是一个单客户端的实现。** 它只监听一个端口，接受一个传入连接，
 * 然后与该客户端进行数据交换，直到连接关闭。它不处理并发连接。
 * 该类封装了 Windows 和 POSIX (Linux/macOS) 系统下 TCP socket 服务器操作的底层细节，
 * 提供统一的接口进行服务器的启动监听、接受连接、数据发送和接收。
 * 支持设置发送和接收超时，并在析构时自动关闭连接和监听 socket。
 * @author 连思鑫（liansixin）
 * @date 2025-4-8
 * @version 1.0
 *
 * ### 核心功能
 * - **单端口监听**: 监听一个指定的 TCP 端口。
 * - **单客户端连接**: 接受并处理一个传入的 TCP 连接。
 * - **跨平台支持**: 封装了 Windows Winsock2 和 POSIX socket API。
 * - **ICommunication 接口实现**: 遵循通用的通信接口，可与其他通信类型互换使用（尽管其单客户端特性限制了互换的场景）。
 * - **数据发送/接收**: 提供阻塞或超时模式的数据发送和接收功能。
 * - **超时设置**: 支持设置在已接受连接上的发送和接收操作的超时时间（映射到平台特定的 socket 选项）。
 * - **资源管理**: 使用 RAII 模式管理 socket 句柄，确保在对象生命周期结束时正确关闭监听 socket 和已接受连接。
 * - **线程安全**: 使用互斥锁保护内部状态和对底层 socket 句柄的访问，支持多线程环境下的安全使用（例如，可以在一个线程中调用 `acceptConnection`，在另一个线程中调用 `send`/`receive`，但需确保只有一个客户端连接）。
 *
 * ### 使用示例
 *
 * @code
 * #include "TcpServer.h"
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
 * uint16_t listen_port = 12345; // 监听端口
 *
 * try {
 * // 创建 TcpServer 对象
 * LSX_LIB::DataTransfer::TcpServer server(listen_port);
 *
 * // 创建监听 socket 并开始监听
 * if (!server.create()) {
 * std::cerr << "创建 TCP 服务器监听 socket 失败!" << std::endl;
 * return 1;
 * }
 * std::cout << "TCP 服务器正在监听端口 " << listen_port << "..." << std::endl;
 *
 * // 设置监听 socket 的接收超时，以便 acceptConnection 可以超时返回
 * // 注意：这里设置的是 listenFd 的超时，影响 acceptConnection 的行为
 * // 如果不设置，acceptConnection 将是阻塞的
 * if (server.setReceiveTimeout(1000)) { // 设置 accept 超时 1 秒
 * std::cout << "设置监听 socket 接收超时 1000ms 成功。" << std::endl;
 * } else {
 * std::cerr << "设置监听 socket 接收超时失败或不支持。" << std::endl;
 * }
 *
 * // 接受客户端连接 (阻塞或超时)
 * std::cout << "等待客户端连接..." << std::endl;
 * while (!g_shutdown_request.load() && !server.acceptConnection()) {
 * // 如果 acceptConnection 超时返回 false，继续循环等待或检查关闭请求
 * if (!g_shutdown_request.load()) {
 * std::cout << "等待连接超时，继续等待..." << std::endl;
 * }
 * }
 *
 * if (g_shutdown_request.load()) {
 * std::cout << "收到关闭请求，停止等待连接。" << std::endl;
 * } else {
 * std::cout << "客户端连接已接受!" << std::endl;
 *
 * // 设置已接受连接的发送/接收超时 (例如 5 秒)
 * if (server.setSendTimeout(5000) && server.setReceiveTimeout(5000)) {
 * std::cout << "设置已连接 socket 超时 5000ms 成功。" << std::endl;
 * } else {
 * std::cerr << "设置已连接 socket 超时失败或不支持。" << std::endl;
 * }
 *
 * // 在连接上进行数据交换
 * std::vector<uint8_t> recv_buffer(256);
 * while (!g_shutdown_request.load()) {
 * int received_bytes = server.receive(recv_buffer.data(), recv_buffer.size());
 * if (received_bytes > 0) {
 * std::cout << "收到 " << received_bytes << " 字节: ";
 * for (int i = 0; i < received_bytes; ++i) {
 * std::cout << static_cast<char>(recv_buffer[i]);
 * }
 * std::cout << std::endl;
 *
 * // 回显数据
 * if (!server.send(recv_buffer.data(), received_bytes)) {
 * std::cerr << "回显数据发送失败!" << std::endl;
 * break; // 发送失败，关闭连接
 * }
 * } else if (received_bytes == 0) {
 * std::cout << "客户端关闭了连接。" << std::endl;
 * break; // 连接关闭，退出循环
 * } else { // received_bytes < 0
 * // 错误或超时
 * if (errno == EWOULDBLOCK || errno == EAGAIN) {
 * // 非阻塞或超时，没有数据，继续循环
 * // std::cout << "接收超时，继续等待..." << std::endl;
 * } else {
 * std::cerr << "数据接收错误，错误码: " << received_bytes << std::endl;
 * break; // 发生错误，关闭连接
 * }
 * }
 * // 小睡片刻，避免 CPU 占用过高 (如果 receive 是非阻塞的)
 * // std::this_thread::sleep_for(std::chrono::milliseconds(10));
 * }
 *
 * // 关闭客户端连接
 * server.closeClientConnection();
 * std::cout << "客户端连接已关闭。" << std::endl;
 * }
 *
 * // TcpServer 对象在 main 函数结束时自动关闭监听 socket
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
 * - **单客户端**: 此类设计为处理单个客户端连接。要处理多个并发连接，需要使用更高级的 I/O 模型（如 select/poll/epoll）或多线程/异步方法，并且可能需要修改 ICommunication 接口或引入新的类来表示每个客户端连接。
 * - **阻塞行为**: `create` 方法本身不阻塞。`acceptConnection` 方法默认是阻塞的，除非在调用 `create` 后设置了监听 socket (`listenFd`) 的接收超时。`send` 和 `receive` 方法的行为受设置的超时影响。
 * - **超时应用**: `setSendTimeout` 和 `setReceiveTimeout` 方法设置的超时仅应用于通过 `acceptConnection` 接受的客户端连接 (`connFd`)，不影响监听 socket (`listenFd`)。要设置 `acceptConnection` 的超时，需要在 `create` 成功后，在 `listenFd` 上单独设置 `SO_RCVTIMEO` 选项，或者使用 `setReceiveTimeout` 方法（如果内部实现将其映射到了 `listenFd`，但在此注释中明确其通常应用于 `connFd`）。
 * - **错误处理**: 错误信息通常会打印到 `std::cerr`，并可能通过 `receive` 方法的负返回值指示。
 * - **线程安全**: `TcpServer` 类内部通过互斥锁保护了成员变量 (`listenFd`, `connFd`) 和对底层 socket 句柄的访问。可以在不同线程中调用其方法，但需要理解其单客户端特性和方法的阻塞行为。例如，在一个线程中调用阻塞的 `acceptConnection`，在另一个线程中调用 `send`/`receive` 是可能的，但必须确保 `acceptConnection` 已经成功返回并且 `connFd` 有效。
 */

// LSXTransportLib: 数据传输工具库（跨平台）
// 命名空间：LSX_LIB

#ifndef LSX_TCP_SERVER_H
#define LSX_TCP_SERVER_H
#pragma once
#include "ICommunication.h" // 包含通信接口基类

// 根据平台包含相应的头文件
#ifdef _WIN32
 #include <winsock2.h> // 包含 Winsock 主要头文件，用于 socket, bind, listen, accept, send, recv, closesocket 等
 #include <ws2tcpip.h> // For socklen_t on some versions, inet_pton (地址转换和类型定义)
 #pragma comment(lib, "ws2_32.lib") // 链接 Winsock 库
#else // POSIX 系统 (Linux, macOS, etc.)
#include <sys/socket.h> // 包含 socket 相关函数和常量 (如 AF_INET, SOCK_STREAM, IPPROTO_TCP, shutdown, SHUT_BOTH, socklen_t)
#include <netinet/in.h> // For sockaddr_in, INADDR_ANY, htons, htonl (地址结构和字节序转换)
#include <arpa/inet.h> // For inet_pton (IP地址字符串转二进制) - 虽然服务器通常绑定 INADDR_ANY，但包含以备用
#include <unistd.h> // For close (关闭文件描述符)
#include <errno.h> // For errno (错误码)
#include <string.h> // For strerror (错误码转字符串)
#include <sys/time.h> // For timeval (POSIX socket timeout option)
#include <sys/select.h> // For timeval definition on some systems (某些系统需要)
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
         * @brief TCP 单客户端服务器实现类。
         * 实现了 ICommunication 接口，用于在 Windows 和 POSIX 系统上创建一个监听特定端口的 TCP 服务器。
         * **重要提示：此实现仅支持接受和处理一个客户端连接。**
         * 它不适用于需要同时处理多个客户端的场景。
         */
        // 注意: 此 TCP 服务器实现是 *单客户端*。
        // 它只监听一个连接并仅与该客户端交互。
        // 对于多客户端服务器，需要完全不同的设计（例如，使用 select/poll/epoll
        // 或线程/异步 I/O），并且 ICommunication 接口可能需要
        // 为每个连接返回新的对象。
        class TcpServer : public ICommunication
        {
        public:
            /**
             * @brief 构造函数。
             * 初始化 TcpServer 对象，设置服务器要监听的端口。
             * 注意：构造函数不创建监听 socket 或开始监听，需要调用 create() 方法。
             *
             * @param port 服务器要监听的端口号。
             */
            explicit TcpServer(uint16_t port);

            /**
             * @brief 创建监听 socket，绑定地址并开始监听。
             * 此方法创建服务器的监听 socket，将其绑定到指定的端口（通常是所有可用网络接口），
             * 并将其设置为监听模式，准备接受传入连接。
             *
             * @return 如果成功创建、绑定并开始监听，返回 true；否则返回 false。
             */
            bool create() override; // 创建监听 socket，绑定并开始监听。不接受连接。

            /**
             * @brief 接受一个待处理的客户端连接。
             * 此方法会阻塞，直到有一个客户端连接到达，或者（如果设置了监听 socket 的超时）超时发生。
             * 如果成功接受连接，内部的客户端 socket 句柄 (`connFd`) 将被更新，并返回 true。
             * 如果超时或发生错误，返回 false。
             * **注意：此方法假定只处理一个客户端连接。** 如果在已连接状态再次调用，行为未定义或可能失败。
             *
             * @return 如果成功接受客户端连接，返回 true；如果超时或发生错误，返回 false。
             */
            bool acceptConnection();

            /**
             * @brief 在当前已接受的客户端连接上发送数据。
             * 将指定缓冲区中的数据通过已建立的客户端连接发送出去。
             * 此方法会尝试发送所有指定字节，可能阻塞直到数据发送完成或发生错误/超时。
             * **注意：此方法仅在成功调用 `acceptConnection` 后并且连接有效时可用。**
             *
             * @param data 指向要发送数据的缓冲区。
             * @param size 要发送的数据的字节数。
             * @return 如果成功发送所有数据，返回 true；否则返回 false。
             */
            bool send(const uint8_t* data, size_t size) override; // 健壮的发送循环

            /**
             * @brief 在当前已接受的客户端连接上接收数据。
             * 从已建立的客户端连接接收数据到指定的缓冲区。
             * 此方法会尝试读取可用数据，其行为受设置的接收超时影响。
             * **注意：此方法仅在成功调用 `acceptConnection` 后并且连接有效时可用。**
             *
             * @param buffer 指向用于存储接收到数据的缓冲区。
             * @param size 接收缓冲区的最大大小（字节）。
             * @return 返回接收到的字节数。
             * - > 0: 成功接收到的字节数。
             * - 0: 客户端关闭了连接，或在非阻塞模式/设置了超时的情况下，当前没有可用的数据。
             * - < 0: 发生错误。
             * 如果没有已接受的连接 (`connFd` 无效)，此方法将报告错误并返回 -1。
             */
            int receive(uint8_t* buffer, size_t size) override; // 修改返回类型

            /**
             * @brief 关闭当前已接受的客户端连接和监听 socket。
             * 释放服务器占用的所有 socket 资源。多次调用是安全的。
             */
            void close() override;

            /**
             * @brief 仅关闭当前已接受的客户端连接。
             * 关闭通过 `acceptConnection` 建立的客户端连接 (`connFd`)，但保留监听 socket (`listenFd`) 开启，
             * 以便可以再次调用 `acceptConnection` 接受新的连接（尽管此实现是单客户端的，通常在连接断开后会停止服务）。
             * 多次调用是安全的。
             */
            void closeClientConnection();

            /**
             * @brief 析构函数。
             * 在对象销毁时自动关闭所有打开的 socket (监听 socket 和客户端连接)，释放资源。
             */
            ~TcpServer();

            /**
             * @brief 设置在已接受连接上的发送操作的超时时间。
             * 尝试将 ICommunication 接口的超时设置映射到平台特定的 socket 发送超时选项 (SO_SNDTIMEO)。
             * **注意：此超时应用于通过 `acceptConnection` 接受的客户端连接 (`connFd`)，不影响监听 socket (`listenFd`)。**
             *
             * @param timeout_ms 超时时间，单位为毫秒。
             * - -1: 设置为阻塞模式。
             * - 0: 设置为非阻塞模式。
             * - > 0: 设置指定的超时时间。
             * @return 如果成功设置超时，返回 true；如果该平台或实现不支持，返回 false。
             */
            bool setSendTimeout(int timeout_ms) override;

            /**
             * @brief 设置在已接受连接上的接收操作的超时时间。
             * 尝试将 ICommunication 接口的超时设置映射到平台特定的 socket 接收超时选项 (SO_RCVTIMEO)。
             * **注意：此超时应用于通过 `acceptConnection` 接受的客户端连接 (`connFd`)，不影响监听 socket (`listenFd`)。**
             * 要设置 `acceptConnection` 的超时，需要在 `create` 成功后，在 `listenFd` 上单独设置 `SO_RCVTIMEO` 选项，或者使用此方法（如果内部实现将其映射到了 `listenFd`）。
             *
             * @param timeout_ms 超时时间，单位为毫秒。
             * - -1: 设置为阻塞模式。
             * - 0: 设置为非阻塞模式。
             * - > 0: 设置指定的超时时间。
             * @return 如果成功设置超时，返回 true；如果该平台或实现不支持，返回 false。
             */
            bool setReceiveTimeout(int timeout_ms) override;

        private: // TcpServer 没有被其他类继承，所以保持 private 即可
#ifdef _WIN32
            /**
             * @brief Windows 监听 socket 句柄。
             * 在 Windows 系统下，用于标识服务器的监听 socket。
             * 初始化为 INVALID_SOCKET。
             */
            SOCKET listenFd = INVALID_SOCKET; // 初始化句柄为无效状态
            /**
             * @brief Windows 客户端连接 socket 句柄。
             * 在 Windows 系统下，用于标识与客户端建立的连接 socket。
             * 初始化为 INVALID_SOCKET。
             */
            SOCKET connFd = INVALID_SOCKET; // 初始化句柄为无效状态
#else
            /**
             * @brief POSIX 监听 socket 文件描述符。
             * 在 POSIX 系统下，用于标识服务器的监听 socket。
             * 初始化为 -1。
             */
            int listenFd = -1; // 初始化句柄为无效状态
            /**
             * @brief POSIX 客户端连接 socket 文件描述符。
             * 在 POSIX 系统下，用于标识与客户端建立的连接 socket。
             * 初始化为 -1。
             */
            int connFd = -1; // 初始化句柄为无效状态
#endif
            /**
             * @brief 服务器本地地址结构体。
             * 存储服务器监听的 IP 地址和端口信息。
             */
            struct sockaddr_in localAddr;

            /**
             * @brief 互斥锁。
             * 用于保护 TcpServer 对象的成员变量 (`listenFd`, `connFd`) 和对底层 socket 句柄的访问，确保线程安全。
             */
            std::mutex mtx; // 用于保护成员变量 (listenFd, connFd) 和 socket 操作的互斥锁
        };
    } // namespace DataTransfer
} // namespace LSX_LIB

#endif // LSX_TCP_SERVER_H
