/**
 * @file CommunicationFactory.h
 * @brief 数据传输工具库 - 通信对象工厂
 * @details 定义了 LSX_LIB::DataTransfer 命名空间下的 CommunicationFactory 类，
 * 用于根据指定的通信类型创建相应的通信对象（如 UDP 客户端/服务器、TCP 客户端/服务器、串口等）。
 * 提供了一个统一的接口来获取不同类型的 ICommunication 实现。
 * @author 连思鑫（liansixin）
 * @date 2025-4-8
 * @version 1.0
 *
 * ### 核心功能
 * - **统一创建接口**: 提供一个静态方法 `create`，简化不同通信对象的实例化过程。
 * - **支持多种通信类型**: 通过 `CommType` 枚举支持常见的网络和串口通信类型。
 * - **参数灵活**: `create` 方法接受通用参数，根据通信类型选择性使用。
 * - **智能指针管理**: 返回 `std::unique_ptr<ICommunication>`，确保资源的自动管理和释放。
 *
 * ### 使用示例
 *
 * @code
 * #include "CommunicationFactory.h"
 * #include "ICommunication.h" // 假设需要包含 ICommunication 头文件
 * #include <iostream>
 *
 * int main() {
 * // 创建一个 UDP 客户端
 * std::unique_ptr<LSX_LIB::DataTransfer::ICommunication> udpClient =
 * LSX_LIB::DataTransfer::CommunicationFactory::create(
 * LSX_LIB::DataTransfer::CommType::UDP_CLIENT, "127.0.0.1", 8080);
 *
 * if (udpClient) {
 * std::cout << "UDP 客户端创建成功!" << std::endl;
 * // 可以使用 udpClient->Send(), udpClient->Receive() 等方法
 * } else {
 * std::cerr << "UDP 客户端创建失败!" << std::endl;
 * }
 *
 * // 创建一个 TCP 服务器
 * std::unique_ptr<LSX_LIB::DataTransfer::ICommunication> tcpServer =
 * LSX_LIB::DataTransfer::CommunicationFactory::create(
 * LSX_LIB::DataTransfer::CommType::TCP_SERVER, "", 9090);
 *
 * if (tcpServer) {
 * std::cout << "TCP 服务器创建成功!" << std::endl;
 * // 可以使用 tcpServer->StartListen(), tcpServer->Accept() 等方法
 * } else {
 * std::cerr << "TCP 服务器创建失败!" << std::endl;
 * }
 *
 * // 创建一个串口对象 (示例参数)
 * std::unique_ptr<LSX_LIB::DataTransfer::ICommunication> serialPort =
 * LSX_LIB::DataTransfer::CommunicationFactory::create(
 * LSX_LIB::DataTransfer::CommType::SERIAL, "", 0, "/dev/ttyS0", 9600);
 *
 * if (serialPort) {
 * std::cout << "串口对象创建成功!" << std::endl;
 * // 可以使用 serialPort->Open(), serialPort->Read(), serialPort->Write() 等方法
 * } else {
 * std::cerr << "串口对象创建失败!" << std::endl;
 * }
 *
 * return 0;
 * }
 * @endcode
 *
 * ### 注意事项
 * - 工厂类只负责对象的创建，不管理对象的生命周期（由返回的 `std::unique_ptr` 管理）。
 * - `create` 方法可能返回 `nullptr`，表示创建失败（例如，不支持的类型、参数错误或资源分配失败）。
 * - 具体的通信实现（如 `UdpClient`, `TcpServer`, `SerialPort` 等）需要在内部被工厂类实例化，
 * 这些实现类需要继承自 `ICommunication` 接口。
 * - 跨平台兼容性依赖于具体的 `ICommunication` 实现类。
 */

// LSXTransportLib: 数据传输工具库（跨平台）
// 命名空间：LSX_LIB

#ifndef LSX_COMMUNICATION_FACTORY_H
#define LSX_COMMUNICATION_FACTORY_H
#pragma once
#include <memory> // 包含 std::unique_ptr, std::make_unique
#include <string> // 包含 std::string
#include "ICommunication.h" // 包含通信接口基类

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
         * @brief 定义了支持的通信类型枚举。
         */
        enum class CommType
        {
            /** UDP 客户端 */
            UDP_CLIENT,
            /** UDP 服务器 */
            UDP_SERVER,
            /** UDP 广播 */
            UDP_BROADCAST,
            /** UDP 多播 */
            UDP_MULTICAST,
            /** TCP 客户端 */
            TCP_CLIENT,
            /** TCP 服务器 */
            TCP_SERVER,
            /** 串口通信 */
            SERIAL
        };

        /**
         * @brief 通信对象工厂类。
         * 提供静态方法用于创建不同类型的通信对象，返回智能指针管理资源。
         */
        class CommunicationFactory
        {
        public:
            /**
             * @brief 创建一个通信对象。
             * 根据指定的通信类型和参数，动态创建一个相应的通信对象。
             *
             * @param type 通信类型，参见 CommType 枚举。
             * @param address IP 地址 (用于 UDP/TCP 客户端/多播)。对于服务器类型通常留空。
             * @param port 端口号 (用于 UDP/TCP)。对于客户端类型，如果是连接模式，需要指定服务器端口；如果是绑定模式，可以指定本地端口或留空由系统分配。
             * @param serialName 串口名称 (用于 SERIAL 类型)。例如 "/dev/ttyS0" 或 "COM1"。
             * @param baudRate 波特率 (用于 SERIAL 类型)。例如 9600, 115200。
             * @param timeout_ms 可选参数，设置发送和接收超时 (毫秒)。
             * - -1 表示阻塞模式 (默认)。
             * - 0 表示非阻塞模式。
             * - > 0 表示指定毫秒数的超时。
             * @return 返回指向创建的通信对象的 unique_ptr。如果创建失败，返回 nullptr。
             */
            static std::unique_ptr<ICommunication> create(CommType type,
                                                          const std::string& address = "", uint16_t port = 0,
                                                          const std::string& serialName = "", int baudRate = 0,
                                                          int timeout_ms = -1);
        };
    } // namespace DataTransfer
} // namespace LSX_LIB

#endif // LSX_COMMUNICATION_FACTORY_H
