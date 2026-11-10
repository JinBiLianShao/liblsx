/**
 * @file IpcSocket.h
 * @brief LIBLSX 工具库 - 本地进程间通信（IPC）模块
 * @details 提供 LSX_LIB::IPC 命名空间下基于 Unix domain socket 的本地进程间
 *          收发能力（POSIX 平台）：
 *  - IpcServer：监听 socket path，accept 单客户端，收发字节流
 *  - IpcClient：连接 socket path，收发字节流
 *
 * 设计要点：
 *  - 与 DataTransfer 的 ICommunication 相同的 create/send/receive/close 风格
 *  - RAII 管理 fd；服务端 create 时自动清理残留的旧 socket 文件
 *  - receive 支持 SO_RCVTIMEO 超时（setReceiveTimeout）
 *  - 默认阻塞；非阻塞需求配合超时使用
 *
 * @author 连思鑫（jinbilianshao）
 * @date 2026-11-10
 * @version 1.0
 *
 * ### 使用示例
 * @code
 * // 服务端
 * LSX_LIB::IPC::IpcServer server("/tmp/lsx_demo.sock");
 * server.create();
 * server.acceptClient();
 * server.send(data, len);
 *
 * // 客户端
 * LSX_LIB::IPC::IpcClient client("/tmp/lsx_demo.sock");
 * client.create();
 * client.send(data, len);
 * int n = client.receive(buf, sizeof(buf));
 * @endcode
 */
#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace LSX_LIB {
namespace IPC {

/**
 * @class IpcServer
 * @brief Unix domain socket 服务端（单客户端模型，与 TcpServer 一致）
 */
class IpcServer
{
public:
    explicit IpcServer(std::string socket_path)
        : socket_path_(std::move(socket_path)) {}

    ~IpcServer() { close(); }

    IpcServer(const IpcServer&) = delete;
    IpcServer& operator=(const IpcServer&) = delete;

    /// @brief 创建监听 socket 并绑定 path；自动清理残留旧 socket 文件
    bool create();

    /// @brief 阻塞接受一个客户端连接（受 setReceiveTimeout 影响可超时）
    bool acceptClient();

    /// @brief 向已连接客户端发送数据
    bool send(const std::uint8_t* data, std::size_t size);

    /// @brief 从已连接客户端接收数据；返回字节数（0=对端关闭/超时，<0=错误）
    int receive(std::uint8_t* buffer, std::size_t size);

    /// @brief 关闭客户端连接（不影响继续 accept 下一个）
    void closeClient();

    /// @brief 关闭监听并清理 socket 文件
    void close();

    /// @brief 设置 accept/receive 超时毫秒（<=0 表示永久阻塞）
    bool setReceiveTimeout(int timeout_ms);

    bool isListening() const { return listen_fd_ >= 0; }
    bool hasClient() const { return conn_fd_ >= 0; }

private:
    bool applyTimeout(int fd);

    std::string socket_path_;
    int listen_fd_ = -1;
    int conn_fd_ = -1;
    int timeout_ms_ = -1;               // <0 永久阻塞
    std::mutex mtx_;                    // 保护 fd 与 accept/send/receive
};

/**
 * @class IpcClient
 * @brief Unix domain socket 客户端
 */
class IpcClient
{
public:
    explicit IpcClient(std::string socket_path)
        : socket_path_(std::move(socket_path)) {}

    ~IpcClient() { close(); }

    IpcClient(const IpcClient&) = delete;
    IpcClient& operator=(const IpcClient&) = delete;

    /// @brief 连接服务端 socket path
    bool create();

    bool send(const std::uint8_t* data, std::size_t size);
    int receive(std::uint8_t* buffer, std::size_t size);

    bool setReceiveTimeout(int timeout_ms);
    void close();

    bool isConnected() const { return fd_ >= 0; }

private:
    std::string socket_path_;
    int fd_ = -1;
    int timeout_ms_ = -1;
    std::mutex mtx_;
};

} // namespace IPC
} // namespace LSX_LIB
