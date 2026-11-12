/**
 * @file example_datatransfer.cpp
 * @brief LIBLSX DataTransfer 模块使用示例
 *
 * 演示内容：
 *   1. UDP 客户端 -> UDP 服务器 回环（同机收发一条消息）
 *   2. TCP 客户端 -> TCP 服务器 回环（面向连接，单客户端模型）
 *   3. CommunicationFactory 工厂按枚举创建通信对象
 *
 * 关键点：
 *   - 所有通信类都实现 ICommunication 接口：create()/send()/receive()/close()，
 *     且 receive() 返回值约定一致：>0 字节数，0 超时/无数据（或 TCP 对端关闭），<0 错误。
 *   - TcpServer 是"单客户端"模型：acceptConnection() 阻塞等一个客户端；
 *     本示例用"服务器先收、客户端后发"的顺序避免 accept 阻塞。
 *   - 示例只用回环地址 127.0.0.1，无需真实网络。
 */

#include "DataTransfer/CommunicationFactory.h"
#include "DataTransfer/TcpClient.h"
#include "DataTransfer/TcpServer.h"
#include "DataTransfer/UdpClient.h"
#include "DataTransfer/UdpServer.h"

#include <cstring>
#include <iostream>
#include <thread>
#include <vector>

namespace dt = LSX_LIB::DataTransfer;

// ---------------------------------------------------------------------------
// 演示 1：UDP 客户端/服务器回环
// ---------------------------------------------------------------------------
static void demo_udp_loopback()
{
    std::cout << "\n===== 演示 1：UDP 回环 =====\n";

    dt::UdpServer server(9001);          // 服务器绑 9001 端口
    dt::UdpClient client("127.0.0.1", 9001);  // 客户端目标是本机 9001

    if (!server.create() || !client.create()) {
        std::cerr << "  UDP 创建失败\n";
        return;
    }

    // 客户端发送一条数据报
    const char* msg = "hello udp";
    client.send(reinterpret_cast<const std::uint8_t*>(msg), std::strlen(msg));

    // 服务器接收（阻塞；数据报未到时 recvfrom 会一直等）
    std::vector<std::uint8_t> buf(256);
    int n = server.receive(buf.data(), buf.size());
    if (n > 0) {
        std::cout << "  [server] 收到 " << n << " 字节: "
                  << std::string(buf.begin(), buf.begin() + n) << "\n";
    }

    client.close();
    server.close();
}

// ---------------------------------------------------------------------------
// 演示 2：TCP 客户端/服务器回环（单客户端模型）
// ---------------------------------------------------------------------------
static void demo_tcp_loopback()
{
    std::cout << "\n===== 演示 2：TCP 回环 =====\n";

    dt::TcpServer server(9002);
    dt::TcpClient client("127.0.0.1", 9002);

    if (!server.create() || !client.create()) {
        std::cerr << "  TCP 创建失败\n";
        return;
    }

    // TcpServer::acceptConnection() 会阻塞到有客户端连入。
    // 所以把它放到子线程里跑，主线程随后让客户端发数据。
    std::thread acceptor([&server]() {
        server.acceptConnection();        // 阻塞等待客户端
        std::cout << "  [server] 已接受客户端连接\n";
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 客户端发送（connect 已在 create 里完成）
    const char* msg = "hello tcp";
    client.send(reinterpret_cast<const std::uint8_t*>(msg), std::strlen(msg));

    // 服务器接收
    std::vector<std::uint8_t> buf(256);
    int n = server.receive(buf.data(), buf.size());
    if (n > 0) {
        std::cout << "  [server] 收到 " << n << " 字节: "
                  << std::string(buf.begin(), buf.begin() + n) << "\n";
    }

    acceptor.join();
    client.close();
    server.close();
}

// ---------------------------------------------------------------------------
// 演示 3：工厂模式创建通信对象
// ---------------------------------------------------------------------------
static void demo_factory()
{
    std::cout << "\n===== 演示 3：CommunicationFactory 工厂 =====\n";

    // 按枚举创建：TCP_CLIENT 目标 127.0.0.1:9003
    auto comm = dt::CommunicationFactory::create(
        dt::CommType::TCP_CLIENT, "127.0.0.1", 9003);

    if (comm) {
        std::cout << "  [factory] 成功创建 TCP_CLIENT 对象\n";
        // 通过 ICommunication 接口统一操作（无需知道具体类型）
        // comm->create(); comm->send(...); comm->receive(...); comm->close();
    } else {
        std::cout << "  [factory] 创建失败\n";
    }
    // unique_ptr 离开作用域自动释放
}

int main()
{
    demo_udp_loopback();
    demo_tcp_loopback();
    demo_factory();
    return 0;
}
