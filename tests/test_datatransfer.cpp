/**
 * @file test_datatransfer.cpp
 * @brief DataTransfer 模块深度测试：TCP 连接回环 / UDP 客户端-服务器 / 广播 / 多播 / 工厂
 */
#include "test_framework.h"

#include <atomic>
#include <chrono>
#include <cstring>
#include <thread>
#include <vector>

#include "CommunicationFactory.h"
#include "TcpClient.h"
#include "TcpServer.h"
#include "UdpClient.h"
#include "UdpServer.h"
#include "UdpBroadcast.h"
#include "UdpMulticast.h"
#include "GlobalErrorMutex.h"

using namespace LSX_LIB;
using namespace std::chrono_literals;

static constexpr uint16_t kBasePort = 45000 + (__TIME__[7] - '0') * 10 + (__TIME__[6] - '0');
static std::atomic<int> port_offset{0};

static uint16_t next_port() { return kBasePort + (port_offset.fetch_add(1) % 40) * 5; }

// ---------- TCP ----------

LSX_TEST(Tcp, ClientServerLoopback) {
    uint16_t port = next_port();
    DataTransfer::TcpServer server(port);
    LSX_EXPECT_TRUE(server.create());

    std::thread server_thread([&]() {
        if (!server.acceptConnection()) return;  // 阻塞等客户端
        uint8_t buf[64];
        int n = server.receive(buf, sizeof(buf));
        if (n > 0) server.send(buf, static_cast<size_t>(n));  // 原样回显
        server.close();  // 回显完关闭，客户端 recv 才能退出
    });

    std::this_thread::sleep_for(100ms);  // 等 listen 就绪
    DataTransfer::TcpClient client("127.0.0.1", port);
    LSX_EXPECT_TRUE(client.create());
    LSX_EXPECT_TRUE(client.setReceiveTimeout(3000));  // 防止回显丢失时无限阻塞
    std::string msg = "hello liblsx tcp";
    LSX_EXPECT_TRUE(client.send(reinterpret_cast<const uint8_t*>(msg.data()), msg.size()));

    uint8_t echo[64] = {};
    int got = client.receive(echo, sizeof(echo));
    server_thread.join();
    LSX_EXPECT_TRUE(got > 0);
    LSX_EXPECT_TRUE(std::memcmp(echo, msg.data(), static_cast<size_t>(got)) == 0);
    LSX_EXPECT_EQ(std::string(reinterpret_cast<char*>(echo), static_cast<size_t>(got)), msg);
    client.close();
}

LSX_TEST(Tcp, LargePayloadTransfer) {
    uint16_t port = next_port();
    DataTransfer::TcpServer server(port);
    LSX_EXPECT_TRUE(server.create());

    std::atomic<size_t> received_total{0};
    std::thread server_thread([&]() {
        if (!server.acceptConnection()) return;
        uint8_t buf[4096];
        while (true) {
            int n = server.receive(buf, sizeof(buf));
            if (n <= 0) break;
            received_total += static_cast<size_t>(n);
        }
        server.close();
    });

    std::this_thread::sleep_for(100ms);
    DataTransfer::TcpClient client("127.0.0.1", port);
    LSX_EXPECT_TRUE(client.create());
    std::vector<uint8_t> chunk(64 * 1024, 0x5A);
    const int reps = 16;  // 共 1MB
    for (int i = 0; i < reps; ++i) {
        LSX_EXPECT_TRUE(client.send(chunk.data(), chunk.size()));
    }
    client.close();  // 关闭触发服务端 recv 返回 0 退出
    server_thread.join();
    LSX_EXPECT_EQ(received_total.load(), chunk.size() * static_cast<size_t>(reps));
}

LSX_TEST(Tcp, ConnectToDeadPortFails) {
    uint16_t port = next_port();
    // 没有服务端监听
    DataTransfer::TcpClient client("127.0.0.1", port);
    LSX_EXPECT_TRUE(!client.create());  // 应优雅失败而非崩溃
}

// ---------- UDP ----------

LSX_TEST(Udp, ClientServerRoundtrip) {
    uint16_t port = next_port();
    auto server = DataTransfer::CommunicationFactory::create(
        DataTransfer::CommType::UDP_SERVER, "", port);
    LSX_EXPECT_TRUE(server != nullptr);
    LSX_EXPECT_TRUE(server->create());

    auto client = DataTransfer::CommunicationFactory::create(
        DataTransfer::CommType::UDP_CLIENT, "127.0.0.1", port);
    LSX_EXPECT_TRUE(client != nullptr);
    LSX_EXPECT_TRUE(client->create());

    std::string msg = "udp ping";
    LSX_EXPECT_TRUE(client->send(reinterpret_cast<const uint8_t*>(msg.data()), msg.size()));

    uint8_t buf[128] = {};
    int n = server->receive(buf, sizeof(buf));  // 服务器等一份报文
    LSX_EXPECT_EQ(n, static_cast<int>(msg.size()));
    LSX_EXPECT_TRUE(std::memcmp(buf, msg.data(), msg.size()) == 0);
}

LSX_TEST(Udp, OversizedPacketRejectedGracefully) {
    uint16_t port = next_port();
    auto server = DataTransfer::CommunicationFactory::create(
        DataTransfer::CommType::UDP_SERVER, "", port);
    server->create();
    auto client = DataTransfer::CommunicationFactory::create(
        DataTransfer::CommType::UDP_CLIENT, "127.0.0.1", port);
    client->create();
    // 超过 sendto 单次上限的包，库内校验应拒绝并返回 false 而非崩溃
    std::vector<uint8_t> huge(10 * 1024 * 1024, 1);
    LSX_EXPECT_TRUE(!client->send(huge.data(), huge.size()));
}

LSX_TEST(Udp, BroadcastSendSucceeds) {
    uint16_t port = next_port();
    auto bc = DataTransfer::CommunicationFactory::create(
        DataTransfer::CommType::UDP_BROADCAST, "255.255.255.255", port);
    LSX_EXPECT_TRUE(bc != nullptr);
    LSX_EXPECT_TRUE(bc->create());
    std::string msg = "broadcast hello";
    LSX_EXPECT_TRUE(bc->send(reinterpret_cast<const uint8_t*>(msg.data()), msg.size()));
}

LSX_TEST(Udp, MulticastJoinAndSend) {
    uint16_t port = next_port();
    auto mcast = DataTransfer::CommunicationFactory::create(
        DataTransfer::CommType::UDP_MULTICAST, "239.255.0.1", port);
    LSX_EXPECT_TRUE(mcast != nullptr);
    LSX_EXPECT_TRUE(mcast->create());
    std::string msg = "multicast hello";
    LSX_EXPECT_TRUE(mcast->send(reinterpret_cast<const uint8_t*>(msg.data()), msg.size()));
}

LSX_TEST(Udp, ServerSendWithoutPeerIsRejected) {
    uint16_t port = next_port();
    auto server = DataTransfer::CommunicationFactory::create(
        DataTransfer::CommType::UDP_SERVER, "", port);
    server->create();
    std::vector<uint8_t> data = {1, 2, 3};
    // UDP 服务器无目标地址，send 应按接口约定拒绝
    LSX_EXPECT_TRUE(!server->send(data.data(), data.size()));
}

LSX_TEST(Factory, SerialOnInvalidDeviceDoesNotCrash) {
    // /dev/null 不是有效串口（无 termios），create 应优雅失败或返回不可用实例——只验证不崩溃
    auto serial = DataTransfer::CommunicationFactory::create(
        DataTransfer::CommType::SERIAL, "", 0, "/dev/null", 115200, 100);
    LSX_EXPECT_TRUE(true);
    if (serial) serial->close();
}

LSX_TEST(Factory, TcpTypeCreatesInstance) {
    auto c = DataTransfer::CommunicationFactory::create(
        DataTransfer::CommType::TCP_CLIENT, "127.0.0.1", next_port());
    LSX_EXPECT_TRUE(c != nullptr);  // 工厂按类型返回正确实例
}
