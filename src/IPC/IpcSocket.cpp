/**
 * @file IpcSocket.cpp
 * @brief IPC 模块实现（POSIX Unix domain socket）
 */
#include "IPC/IpcSocket.h"

#include <cerrno>
#include <cstdio>
#include <cstring>

#include <fcntl.h>
#include <sys/select.h>
#include <sys/time.h>

namespace LSX_LIB {
namespace IPC {

namespace
{
/// 向 fd 应用 SO_RCVTIMEO/SO_SNDTIMEO 超时（timeout_ms <= 0 表示阻塞）
bool applySocketTimeout(int fd, int timeout_ms)
{
    if (fd < 0 || timeout_ms <= 0)
    {
        return true;   // 阻塞模式无需设置
    }
    timeval tv{};
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
    {
        return false;
    }
    if (setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0)
    {
        return false;
    }
    return true;
}

/// 单次 send 全量发送循环；返回是否全部发出
bool sendAll(int fd, const std::uint8_t* data, std::size_t size)
{
    std::size_t sent = 0;
    while (sent < size)
    {
        ssize_t n = ::send(fd, data + sent, size - sent, MSG_NOSIGNAL);
        if (n < 0)
        {
            if (errno == EINTR) continue;
            return false;
        }
        // send 返回 0（对端关闭/缓冲满且非阻塞）时不得视为进展，
        // 否则 sent 不前进会形成死循环。
        if (n == 0) return false;
        sent += static_cast<std::size_t>(n);
    }
    return true;
}
} // namespace

// ---------- IpcServer ----------

bool IpcServer::create()
{
    std::lock_guard<std::mutex> lk(mtx_);
    if (listen_fd_ >= 0)
    {
        return true;   // 已在监听
    }

    // 清理残留的旧 socket 文件（上次进程异常退出的遗留）
    ::remove(socket_path_.c_str());

    int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0)
    {
        return false;
    }

    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    if (socket_path_.size() >= sizeof(addr.sun_path))
    {
        ::close(fd);
        return false;   // path 过长
    }
    std::strncpy(addr.sun_path, socket_path_.c_str(), sizeof(addr.sun_path) - 1);

    if (::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0)
    {
        ::close(fd);
        return false;
    }
    if (::listen(fd, 4) < 0)
    {
        ::close(fd);
        ::remove(socket_path_.c_str());
        return false;
    }

    listen_fd_ = fd;
    applySocketTimeout(listen_fd_, timeout_ms_);   // 影响 accept
    return true;
}

bool IpcServer::acceptClient()
{
    std::lock_guard<std::mutex> lk(mtx_);
    if (listen_fd_ < 0)
    {
        return false;
    }
    closeClient();   // 单客户端模型：accept 前关闭旧连接（同锁内调用，安全）

    int fd = ::accept(listen_fd_, nullptr, nullptr);
    if (fd < 0)
    {
        return false;   // 超时（EAGAIN）或错误
    }
    applySocketTimeout(fd, timeout_ms_);
    conn_fd_ = fd;
    return true;
}

bool IpcServer::send(const std::uint8_t* data, std::size_t size)
{
    std::lock_guard<std::mutex> lk(mtx_);
    if (conn_fd_ < 0)
    {
        return false;
    }
    return sendAll(conn_fd_, data, size);
}

int IpcServer::receive(std::uint8_t* buffer, std::size_t size)
{
    std::lock_guard<std::mutex> lk(mtx_);
    if (conn_fd_ < 0)
    {
        return -1;
    }
    ssize_t n = ::recv(conn_fd_, buffer, size, 0);
    if (n < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            return 0;   // 超时
        }
        return -1;
    }
    return static_cast<int>(n);   // 0 = 对端关闭
}

void IpcServer::closeClient()
{
    // 仅在已持有 mtx_ 的成员函数内调用
    if (conn_fd_ >= 0)
    {
        ::shutdown(conn_fd_, SHUT_RDWR);
        ::close(conn_fd_);
        conn_fd_ = -1;
    }
}

void IpcServer::close()
{
    std::lock_guard<std::mutex> lk(mtx_);
    closeClient();
    if (listen_fd_ >= 0)
    {
        ::close(listen_fd_);
        listen_fd_ = -1;
    }
    ::remove(socket_path_.c_str());
}

bool IpcServer::setReceiveTimeout(int timeout_ms)
{
    std::lock_guard<std::mutex> lk(mtx_);
    timeout_ms_ = timeout_ms;
    return applySocketTimeout(listen_fd_, timeout_ms_) &&
           (conn_fd_ < 0 || applySocketTimeout(conn_fd_, timeout_ms_));
}

bool IpcServer::applyTimeout(int fd)
{
    return applySocketTimeout(fd, timeout_ms_);
}

// ---------- IpcClient ----------

bool IpcClient::create()
{
    std::lock_guard<std::mutex> lk(mtx_);
    if (fd_ >= 0)
    {
        return true;   // 已连接
    }

    int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0)
    {
        return false;
    }

    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    if (socket_path_.size() >= sizeof(addr.sun_path))
    {
        ::close(fd);
        return false;
    }
    std::strncpy(addr.sun_path, socket_path_.c_str(), sizeof(addr.sun_path) - 1);

    if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0)
    {
        ::close(fd);
        return false;   // 服务端未启动 / path 不存在
    }

    fd_ = fd;
    applySocketTimeout(fd_, timeout_ms_);
    return true;
}

bool IpcClient::send(const std::uint8_t* data, std::size_t size)
{
    std::lock_guard<std::mutex> lk(mtx_);
    if (fd_ < 0)
    {
        return false;
    }
    return sendAll(fd_, data, size);
}

int IpcClient::receive(std::uint8_t* buffer, std::size_t size)
{
    std::lock_guard<std::mutex> lk(mtx_);
    if (fd_ < 0)
    {
        return -1;
    }
    ssize_t n = ::recv(fd_, buffer, size, 0);
    if (n < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            return 0;
        }
        return -1;
    }
    return static_cast<int>(n);
}

bool IpcClient::setReceiveTimeout(int timeout_ms)
{
    std::lock_guard<std::mutex> lk(mtx_);
    timeout_ms_ = timeout_ms;
    return applySocketTimeout(fd_, timeout_ms_);
}

void IpcClient::close()
{
    std::lock_guard<std::mutex> lk(mtx_);
    if (fd_ >= 0)
    {
        ::shutdown(fd_, SHUT_RDWR);
        ::close(fd_);
        fd_ = -1;
    }
}

} // namespace IPC
} // namespace LSX_LIB
