/**
 * @file test_ipc.cpp
 * @brief IPC 模块深度测试：UDS 回环/超时/断线/残留 socket 清理
 */
#include "test_framework.h"

#include <atomic>
#include <chrono>
#include <cstring>
#include <thread>
#include <vector>

#include "IPC/IpcSocket.h"

using namespace LSX_LIB;
using namespace std::chrono_literals;

static std::string sock_path(const char* tag) {
    return std::string("/tmp/lsx_test_ipc_") + tag + "_" +
           std::to_string(static_cast<long>(::getpid())) + ".sock";
}

LSX_TEST(IpcSocket, ClientServerRoundtrip) {
    auto path = sock_path("rt");
    IPC::IpcServer server(path);
    LSX_EXPECT_TRUE(server.create());
    LSX_EXPECT_TRUE(server.isListening());

    std::thread server_thread([&]() {
        if (!server.acceptClient()) return;
        std::uint8_t buf[64];
        int n = server.receive(buf, sizeof(buf));
        if (n > 0) server.send(buf, static_cast<std::size_t>(n));  // 回显
    });

    std::this_thread::sleep_for(50ms);  // 等 listen 就绪
    IPC::IpcClient client(path);
    LSX_EXPECT_TRUE(client.create());
    LSX_EXPECT_TRUE(client.isConnected());

    std::string msg = "ipc ping";
    LSX_EXPECT_TRUE(client.send(reinterpret_cast<const std::uint8_t*>(msg.data()), msg.size()));

    std::uint8_t echo[64] = {};
    int got = client.receive(echo, sizeof(echo));
    server_thread.join();
    LSX_EXPECT_TRUE(got > 0);
    LSX_EXPECT_EQ(std::string(reinterpret_cast<char*>(echo), static_cast<size_t>(got)), msg);
    client.close();
}

LSX_TEST(IpcSocket, LargePayloadRoundtrip) {
    auto path = sock_path("big");
    IPC::IpcServer server(path);
    LSX_EXPECT_TRUE(server.create());

    std::atomic<size_t> received_total{0};
    std::thread server_thread([&]() {
        if (!server.acceptClient()) return;
        std::uint8_t buf[4096];
        while (true) {
            int n = server.receive(buf, sizeof(buf));
            if (n <= 0) break;
            received_total += static_cast<size_t>(n);
        }
    });

    std::this_thread::sleep_for(50ms);
    IPC::IpcClient client(path);
    LSX_EXPECT_TRUE(client.create());
    std::vector<std::uint8_t> chunk(64 * 1024, 0xA5);
    for (int i = 0; i < 16; ++i)  // 1MB
    {
        LSX_EXPECT_TRUE(client.send(chunk.data(), chunk.size()));
    }
    client.close();  // 关闭触发服务端退出
    server_thread.join();
    LSX_EXPECT_EQ(received_total.load(), chunk.size() * 16u);
}

LSX_TEST(IpcSocket, ConnectToNonexistentServerFails) {
    IPC::IpcClient client(sock_path("ghost"));
    LSX_EXPECT_TRUE(!client.create());  // 无服务端，优雅失败
}

LSX_TEST(IpcSocket, ReceiveTimeoutReturnsZero) {
    auto path = sock_path("timeout");
    IPC::IpcServer server(path);
    LSX_EXPECT_TRUE(server.create());
    LSX_EXPECT_TRUE(server.setReceiveTimeout(100));  // 100ms 超时

    // 无人连接时 accept 超时返回 false（不永久挂起）
    LSX_EXPECT_TRUE(!server.acceptClient());

    // 有连接但无数据时 receive 超时返回 0
    IPC::IpcClient client(path);
    LSX_EXPECT_TRUE(client.create());
    LSX_EXPECT_TRUE(server.acceptClient());
    std::uint8_t buf[16];
    int n = server.receive(buf, sizeof(buf));
    LSX_EXPECT_EQ(n, 0);  // 超时语义
}

LSX_TEST(IpcSocket, StaleSocketFileCleanedOnCreate) {
    auto path = sock_path("stale");
    // 模拟上次进程遗留的 socket 文件
    { FILE* f = fopen(path.c_str(), "w"); fputs("garbage", f); fclose(f); }

    IPC::IpcServer server(path);
    LSX_EXPECT_TRUE(server.create());   // 自动清理残留并成功绑定
    server.close();
}

LSX_TEST(IpcSocket, ServerCloseRemovesSocketFile) {
    auto path = sock_path("rm");
    {
        IPC::IpcServer server(path);
        server.create();
        LSX_EXPECT_TRUE(server.isListening());
    }
    // 析构后 socket 文件应被清理
    FILE* f = fopen(path.c_str(), "r");
    LSX_EXPECT_TRUE(f == nullptr);
}

LSX_TEST(IpcSocket, ClientDetectsServerClose) {
    auto path = sock_path("disc");
    auto* server = new IPC::IpcServer(path);
    server->create();
    server->setReceiveTimeout(200);   // accept 200ms 内完成，避免测试挂起

    // accept 放到独立线程，客户端在主线程连接
    std::thread acceptor([server]() { server->acceptClient(); });
    std::this_thread::sleep_for(50ms);

    IPC::IpcClient client(path);
    LSX_EXPECT_TRUE(client.create());
    client.send(reinterpret_cast<const std::uint8_t*>("x"), 1);
    acceptor.join();

    delete server;            // 服务端进程级销毁
    std::this_thread::sleep_for(30ms);

    std::uint8_t buf[16];
    int n = client.receive(buf, sizeof(buf));   // 对端关闭 → 0（或 -1）
    LSX_EXPECT_TRUE(n <= 0);
    client.close();
}

LSX_TEST(IpcSocket, SequentialClientsOnSameServer) {
    auto path = sock_path("seq");
    IPC::IpcServer server(path);
    LSX_EXPECT_TRUE(server.create());

    for (int round = 0; round < 3; ++round) {
        std::thread acceptor([&]() {
            if (server.acceptClient()) {
                std::uint8_t buf[16];
                int n = server.receive(buf, sizeof(buf));
                if (n > 0) server.send(buf, static_cast<std::size_t>(n));
            }
        });
        std::this_thread::sleep_for(50ms);

        IPC::IpcClient client(path);
        LSX_EXPECT_TRUE(client.create());
        std::string msg = "round" + std::to_string(round);
        LSX_EXPECT_TRUE(client.send(reinterpret_cast<const std::uint8_t*>(msg.data()), msg.size()));
        std::uint8_t echo[16] = {};
        int got = client.receive(echo, sizeof(echo));
        acceptor.join();
        LSX_EXPECT_EQ(std::string(reinterpret_cast<char*>(echo), static_cast<size_t>(got)), msg);
        client.close();
        server.closeClient();  // 允许下一个客户端接入
    }
    server.close();
}

LSX_TEST(IpcSocket, SendBeforeClientAcceptedFails) {
    auto path = sock_path("noconn");
    IPC::IpcServer server(path);
    LSX_EXPECT_TRUE(server.create());
    LSX_EXPECT_TRUE(!server.send(reinterpret_cast<const std::uint8_t*>("x"), 1));  // 无客户端
    LSX_EXPECT_EQ(server.receive(nullptr, 0), -1);
}
