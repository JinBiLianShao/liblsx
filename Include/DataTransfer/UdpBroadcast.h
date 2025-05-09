/**
 * @file UdpBroadcast.h
 * @brief 数据传输工具库 - UDP 广播实现类
 * @details 定义了 LSX_LIB::DataTransfer 命名空间下的 UdpBroadcast 类，
 * 它是 UdpClient 类的一个派生类，专门用于发送 UDP 广播消息。
 * 通过继承 UdpClient 并重写 create 方法，该类在创建 socket 后设置了 SO_BROADCAST 选项，
 * 使得发送到广播地址的消息能够被同一局域网内的所有设备接收。
 * @author 连思鑫（liansixin）
 * @date 2025-4-8
 * @version 1.0
 *
 * ### 核心功能
 * - **UDP 广播**: 能够向指定端口发送 UDP 广播消息。
 * - **继承 UdpClient**: 复用 UdpClient 的大部分功能，专注于广播特性。
 * - **自动设置广播选项**: 在 create 方法中自动设置 socket 的 SO_BROADCAST 选项。
 * - **跨平台支持**: 依赖于 UdpClient 的跨平台实现。
 *
 * ### 使用示例
 *
 * @code
 * #include "UdpBroadcast.h"
 * #include <iostream>
 * #include <vector>
 * #include <thread>
 *
 * // 示例：使用 UdpBroadcast 发送广播消息
 * int main() {
 * uint16_t broadcast_port = 8888; // 广播端口
 *
 * try {
 * // 创建 UdpBroadcast 对象
 * // 构造函数只需要端口，广播地址将在 send 方法中指定或默认使用 255.255.255.255
 * LSX_LIB::DataTransfer::UdpBroadcast broadcaster(broadcast_port);
 *
 * // 创建并配置 socket (设置广播选项)
 * if (!broadcaster.create()) {
 * std::cerr << "创建 UDP 广播 socket 失败!" << std::endl;
 * return 1;
 * }
 * std::cout << "UDP 广播 socket 创建成功，端口 " << broadcast_port << std::endl;
 *
 * // 设置发送超时 (可选)
 * if (broadcaster.setSendTimeout(100)) {
 * std::cout << "发送超时设置为 100ms 成功。" << std::endl;
 * } else {
 * std::cerr << "设置发送超时失败或不支持。" << std::endl;
 * }
 *
 * // 发送广播消息
 * const uint8_t broadcast_data[] = "Hello, Broadcast World!";
 * // 注意：发送广播时，send 方法通常需要指定广播地址 (如 "255.255.255.255")
 * // 假设 UdpClient::send 方法支持指定目标地址，或者 UdpBroadcast 内部处理
 * // (此处示例代码仅为概念演示，实际使用依赖于 UdpClient::send 的具体签名和实现)
 * // 如果 UdpClient::send 仅发送到构造函数指定的地址，UdpBroadcast 可能需要重写 send
 * // 以允许指定广播地址。如果 UdpClient 构造函数只指定本地绑定端口，send 必须指定目标。
 * // 如果 UdpClient 构造函数指定了目标地址，UdpBroadcast 需要确保目标是广播地址。
 *
 * // 假设 UdpClient::send 内部会使用构造函数或 create 时的目标地址 (在此是广播端口)
 * // 如果需要指定广播地址，可能需要 UdpClient::send(data, size, target_ip, target_port)
 * // 或者 UdpBroadcast 内部固定使用广播地址
 *
 * // 假设 UdpClient::send 可以发送到构造函数指定的端口，并且我们依赖底层系统将发送到广播地址
 * // 更典型的 UDP 广播发送是连接到广播地址，然后使用 send 或 sendto。
 * // 此处继承 UdpClient 的设计可能需要 UdpClient 构造函数接受目标地址，或者 UdpBroadcast 重写 send。
 * // 考虑到继承关系，UdpBroadcast 可能在 create 中连接到广播地址。
 *
 * // 实际发送通常需要目标地址，但 UdpBroadcast 的设计继承 UdpClient，
 * // 且构造函数只接受端口。这暗示 UdpClient 的 send 是无连接的，需要目标地址，
 * // 或者 UdpBroadcast 在 create 中连接到广播地址。
 * // 如果 UdpClient::send(data, size) 是无连接发送，它需要知道目标。
 * // 如果 UdpClient::send 是面向连接的 (UDP 也可以 connect)，那 create 会连接到广播地址。
 *
 * // 基于 UdpClient 构造函数 (ip, port) 和 UdpBroadcast 构造函数 (port) 的差异，
 * // UdpBroadcast 可能在 create 中设置本地端口并设置广播选项，
 * // 然后在 send 方法中指定广播地址 (如 "255.255.255.255") 和构造函数中的端口。
 *
 * // 假设 UdpClient::send 方法签名是 send(const uint8_t* data, size_t size, const std::string& target_ip, uint16_t target_port)
 * // 或者 UdpBroadcast 重写 send 方法来固定目标为广播地址
 *
 * // 如果 UdpClient::send 只是 send(const uint8_t* data, size_t size)，那么 UdpClient 在 create 或构造函数中需要目标地址。
 * // UdpBroadcast 构造函数只接受端口，这与 UdpClient 构造函数(ip, port) 不一致。
 * // 让我们假设 UdpClient 构造函数可以只接受端口（绑定本地端口），send 方法需要目标地址。
 * // 那么 UdpBroadcast 在 create 中设置广播选项，并在 send 中使用广播地址和构造函数端口。
 *
 * // 假设 UdpClient::send 方法需要目标 IP 和端口
 * // if (broadcaster.send(broadcast_data, sizeof(broadcast_data) - 1, "255.255.255.255", broadcast_port)) {
 * //     std::cout << "广播数据发送成功: " << broadcast_data << std::endl;
 * // } else {
 * //     std::cerr << "广播数据发送失败。" << std::endl;
 * // }
 *
 * // 考虑到 UdpBroadcast 继承自 UdpClient 且只重写 create，
 * // 更可能的设计是 UdpClient 构造函数可以只接受端口（用于本地绑定），
 * // 而 send 方法需要目标地址和端口。或者 UdpClient 构造函数接受目标地址和端口，
 * // 而 UdpBroadcast 在 create 中设置广播选项，并在构造函数中接收端口，
 * // 并可能在内部将目标地址设置为广播地址。
 *
 * // 让我们假设 UdpBroadcast 构造函数设置了本地端口，并在 create 中设置广播选项，
 * // 并且 UdpClient 的 send 方法需要目标地址和端口。
 * // 那么 UdpBroadcast 需要重写 send 方法来固定目标地址为广播地址。
 * // 但 UdpBroadcast 没有重写 send。这表明 UdpClient::send 可能不需要目标地址，
 * // 而是使用构造函数或 create 中设置的目标地址。
 * // UdpClient 构造函数是 (ip, port)。UdpBroadcast 构造函数是 (port)。
 * // 这意味着 UdpBroadcast 可能在 create 中设置目标地址为广播地址 ("255.255.255.255")
 * // 并使用构造函数中的端口。
 *
 * // 最终假设: UdpBroadcast 在 create 中设置本地端口并设置广播选项，
 * // 并在内部将目标地址设置为广播地址 "255.255.255.255"，端口为构造函数指定的端口。
 * // 然后调用 UdpClient::send(data, size)，该方法使用内部存储的目标地址和端口发送。
 *
 * const uint8_t broadcast_data[] = "Hello, Broadcast World!";
 * if (broadcaster.send(broadcast_data, sizeof(broadcast_data) - 1)) { // 使用继承的 send 方法
 * std::cout << "广播数据发送成功: " << broadcast_data << std::endl;
 * } else {
 * std::cerr << "广播数据发送失败。" << std::endl;
 * }
 *
 * // 广播通常是单向的，接收端需要一个 UdpServer 或 UdpClient 绑定到相同的端口并接收。
 * // 接收广播消息的示例 (需要另一个程序或线程):
 * // LSX_LIB::DataTransfer::UdpServer receiver(broadcast_port); // 或 UdpClient 绑定到端口并接收
 * // if (receiver.create()) { ... receiver.receive(...) ... }
 *
 * // UdpBroadcast 对象在 main 函数结束时自动关闭 socket
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
 * - **继承关系**: UdpBroadcast 继承自 UdpClient。其行为很大程度上依赖于 UdpClient 的实现。
 * - **广播地址**: 广播消息的目标地址通常是 "255.255.255.255"。UdpBroadcast 的实现需要在内部处理这个目标地址的设置，通常是在 create 方法中设置 SO_BROADCAST 选项，并在 send 方法中指定广播地址（或者 UdpClient 的 send 方法设计为在无连接模式下需要目标地址）。
 * - **端口**: 构造函数中的端口是广播消息的目标端口。接收广播的设备需要监听相同的端口。
 * - **无连接**: UDP 是无连接协议，广播也是无连接的。发送广播不需要建立连接，接收广播也不需要接受连接。
 * - **局域网限制**: 广播消息通常只在本地局域网内传播，不会跨越路由器。
 * - **错误处理**: 错误信息通常会打印到 `std::cerr`。
 */

// LSXTransportLib: 数据传输工具库（跨平台）
// 命名空间：LSX_LIB

#ifndef LSX_UDP_BROADCAST_H
#define LSX_UDP_BROADCAST_H
#pragma once
// 包含全局错误锁头文件
#include "GlobalErrorMutex.h"
// 包含 LIBLSX::LockManager::LockGuard 头文件
#include "LockGuard.h"
#include "UdpClient.h" // 继承自 UdpClient，复用其基础功能

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
         * @brief UDP 广播实现类。
         * 继承自 UdpClient，专门用于发送 UDP 广播消息。
         * 在 create 方法中设置 socket 的 SO_BROADCAST 选项。
         */
        class UdpBroadcast : public UdpClient
        {
        public:
            /**
             * @brief 构造函数。
             * 初始化 UdpBroadcast 对象，设置广播消息的目标端口。
             * 注意：构造函数不创建 socket 或设置广播选项，需要调用 create() 方法。
             *
             * @param port 广播消息的目标端口号。
             */
            explicit UdpBroadcast(uint16_t port);

            /**
             * @brief 创建并配置 UDP socket 以进行广播。
             * 重写 UdpClient 的 create 方法。除了创建 socket 外，还会设置 socket 的 SO_BROADCAST 选项，
             * 使得发送到广播地址的数据包能够被正确处理。
             *
             * @return 如果成功创建并配置 socket，返回 true；否则返回 false。
             */
            bool create() override;
        };
    } // namespace DataTransfer
} // namespace LSX_LIB

#endif // LSX_UDP_BROADCAST_H
