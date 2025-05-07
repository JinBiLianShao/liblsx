/**
 * @file ICommunication.h
 * @brief 数据传输工具库 - 通信接口基类
 * @details 定义了 LSX_LIB::DataTransfer 命名空间下的抽象基类 ICommunication，
 * 提供了所有具体通信实现（如 UDP、TCP、串口）必须遵循的标准接口。
 * 这个接口抽象了底层通信的细节，使得上层应用可以使用统一的方式进行数据发送、接收和连接管理。
 * @author 连思鑫（liansixin）
 * @date 2025-4-8
 * @version 1.0
 *
 * ### 核心功能
 * - **接口标准化**: 定义了一套通用的通信操作接口，包括连接创建、数据发送、数据接收和连接关闭。
 * - **多态支持**: 作为抽象基类，允许通过基类指针操作不同类型的具体通信对象。
 * - **超时配置**: 提供设置发送和接收超时的可选接口。
 * - **资源管理**: 虚析构函数确保派生类资源的正确释放。
 *
 * ### 使用示例
 *
 * @code
 * #include "ICommunication.h"
 * #include "CommunicationFactory.h" // 假设需要包含工厂类头文件来创建具体对象
 * #include <iostream>
 * #include <vector>
 *
 * // 示例：使用 ICommunication 接口发送和接收数据
 * void communicate_with(LSX_LIB::DataTransfer::ICommunication* comm) {
 * if (!comm) {
 * std::cerr << "通信对象无效!" << std::endl;
 * return;
 * }
 *
 * // 尝试创建连接
 * if (!comm->create()) {
 * std::cerr << "创建连接失败!" << std::endl;
 * return;
 * }
 * std::cout << "连接创建成功." << std::endl;
 *
 * // 发送数据
 * const uint8_t send_data[] = "Hello, World!";
 * if (comm->send(send_data, sizeof(send_data) - 1)) { // -1 排除 null 终止符
 * std::cout << "数据发送成功." << std::endl;
 * } else {
 * std::cerr << "数据发送失败." << std::endl;
 * }
 *
 * // 接收数据
 * std::vector<uint8_t> recv_buffer(1024);
 * int received_bytes = comm->receive(recv_buffer.data(), recv_buffer.size());
 * if (received_bytes > 0) {
 * std::cout << "收到 " << received_bytes << " 字节数据: ";
 * for (int i = 0; i < received_bytes; ++i) {
 * std::cout << static_cast<char>(recv_buffer[i]);
 * }
 * std::cout << std::endl;
 * } else if (received_bytes == 0) {
 * std::cout << "接收超时或连接关闭。" << std::endl;
 * } else {
 * std::cerr << "数据接收错误，错误码: " << received_bytes << std::endl;
 * }
 *
 * // 关闭连接
 * comm->close();
 * std::cout << "连接已关闭." << std::endl;
 * }
 *
 * int main() {
 * // 使用工厂创建 UDP 客户端对象，并通过 ICommunication 接口使用
 * std::unique_ptr<LSX_LIB::DataTransfer::ICommunication> udpClient =
 * LSX_LIB::DataTransfer::CommunicationFactory::create(
 * LSX_LIB::DataTransfer::CommType::UDP_CLIENT, "127.0.0.1", 8080);
 *
 * communicate_with(udpClient.get());
 *
 * // unique_ptr 在 main 函数结束时自动释放 udpClient 对象
 *
 * return 0;
 * }
 * @endcode
 *
 * ### 注意事项
 * - **纯虚函数**: `create`, `send`, `receive`, `close` 是纯虚函数，派生类必须提供具体实现。
 * - **超时函数**: `setSendTimeout` 和 `setReceiveTimeout` 提供了默认实现（返回 false），具体通信类型如果支持超时设置，应覆盖这些方法。
 * - **返回值**: `receive` 方法的返回值约定用于指示接收状态（成功字节数、超时/关闭、错误）。
 * - **线程安全**: `ICommunication` 接口本身不保证线程安全。具体的派生类实现需要考虑其方法的线程安全性。通常，同一个通信对象的发送和接收操作可能需要在外部进行同步。
 */

// LSXTransportLib: 数据传输工具库（跨平台）
// 命名空间：LSX_LIB

#ifndef LSX_I_COMMUNICATION_H
#define LSX_I_COMMUNICATION_H
#pragma once
#include <cstdint> // 包含 uint8_t
#include <cstddef> // 包含 size_t

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
         * @brief 抽象通信接口类。
         * 定义了所有具体通信实现（如 UDP、TCP、串口）必须遵循的标准接口。
         * 这是一个纯虚类，不能直接实例化。
         */
        class ICommunication
        {
        public:
            /**
             * @brief 初始化或打开通信连接。
             * 例如，对于网络通信，可能涉及创建 socket 并绑定地址；对于串口，可能涉及打开串口设备。
             *
             * @return 如果连接/初始化成功，返回 true；否则返回 false。
             */
            virtual bool create() = 0;

            /**
             * @brief 发送数据。
             * 尝试将指定缓冲区中的数据发送出去。
             *
             * @param data 指向要发送数据的缓冲区。
             * @param size 要发送的数据的字节数。
             * @return 如果成功发送所有数据，返回 true；否则返回 false。
             * 注意：具体实现可能会根据超时设置阻塞或非阻塞。
             */
            virtual bool send(const uint8_t* data, size_t size) = 0;

            /**
             * @brief 接收数据。
             * 尝试从通信通道接收数据到指定的缓冲区。
             *
             * @param buffer 指向用于存储接收到数据的缓冲区。
             * @param size 接收缓冲区的最大大小（字节）。
             * @return 返回接收到的字节数。
             * - > 0: 成功接收到的字节数。
             * - 0: 连接正常关闭 (仅适用于面向连接的协议如 TCP)，或在非阻塞模式/设置了超时的情况下，当前没有可用的数据。
             * - < 0: 发生错误。具体的负值可以用来表示不同的错误码（由具体实现定义）。
             */
            virtual int receive(uint8_t* buffer, size_t size) = 0;

            /**
             * @brief 关闭通信连接并释放相关资源。
             * 例如，关闭 socket 文件描述符或串口句柄。
             * 多次调用此方法应该是安全的，后续调用应无副作用。
             */
            virtual void close() = 0;

            /**
             * @brief 设置发送操作的超时时间。
             * 此方法是可选的，并非所有通信类型都支持。
             *
             * @param timeout_ms 超时时间，单位为毫秒。
             * - -1: 设置为阻塞模式（默认）。
             * - 0: 设置为非阻塞模式（立即返回）。
             * - > 0: 设置指定的超时时间。
             * @return 如果成功设置超时，返回 true；如果该通信类型不支持超时设置，返回 false。
             */
            virtual bool setSendTimeout(int timeout_ms) { return false; }

            /**
             * @brief 设置接收操作的超时时间。
             * 此方法是可选的，并非所有通信类型都支持。
             *
             * @param timeout_ms 超时时间，单位为毫秒。
             * - -1: 设置为阻塞模式（默认）。
             * - 0: 设置为非阻塞模式（立即返回）。
             * - > 0: 设置指定的超时时间。
             * @return 如果成功设置超时，返回 true；如果该通信类型不支持超时设置，返回 false。
             */
            virtual bool setReceiveTimeout(int timeout_ms) { return false; }

            /**
             * @brief 虚析构函数。
             * 确保通过基类指针删除派生类对象时，能够正确调用派生类的析构函数，释放所有资源。
             */
            virtual ~ICommunication()
            {
            }
        };
    } // namespace DataTransfer
} // namespace LSX_LIB

#endif // LSX_I_COMMUNICATION_H
