/**
 * @file example_ipc.cpp
 * @brief LIBLSX IPC 模块使用示例（POSIX Unix domain socket）
 *
 * 演示内容：
 *   IpcServer / IpcClient 本机进程间通信回环（同进程内起服务端线程 + 客户端）。
 *
 * 关键点：
 *   - create() 会清理残留 socket 文件；close() 自动删除 socket 文件。
 *   - acceptClient() 阻塞等待一个客户端；receive 支持超时（setReceiveTimeout）。
 *   - 单客户端模型：accept 前会关闭旧连接。
 */

#include "IPC/IpcSocket.h"

#include <cstring>
#include <iostream>
#include <thread>
#include <vector>

namespace ipc = LSX_LIB::IPC;

int main()
{
    std::cout << "===== IPC（Unix domain socket）示例 =====\n";

    const std::string sock_path = "/tmp/lsx_ipc_example.sock";

    // ---- 服务端（在子线程里跑 accept + receive）----
    ipc::IpcServer server(sock_path);
    if (!server.create()) {
        std::cerr << "  服务端创建失败\n";
        return 1;
    }
    server.setReceiveTimeout(2000);    // 接收超时 2 秒

    std::thread server_thread([&server]() {
        if (!server.acceptClient()) {
            std::cerr << "  [server] accept 失败或超时\n";
            return;
        }
        std::cout << "  [server] 已接受客户端\n";
        std::vector<std::uint8_t> buf(256);
        int n = server.receive(buf.data(), buf.size());
        if (n > 0) {
            std::cout << "  [server] 收到 " << n << " 字节: "
                      << std::string(buf.begin(), buf.begin() + n) << "\n";
        }
    });

    // ---- 客户端 ----
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ipc::IpcClient client(sock_path);
    if (client.create()) {
        const char* msg = "hello ipc";
        client.send(reinterpret_cast<const std::uint8_t*>(msg), std::strlen(msg));
        std::cout << "  [client] 已发送\n";
        client.close();
    } else {
        std::cerr << "  [client] 连接失败（服务端未就绪？）\n";
    }

    server_thread.join();
    server.close();                   // 关闭并删除 socket 文件
    std::cout << "  IPC 回环完成\n";
    return 0;
}
