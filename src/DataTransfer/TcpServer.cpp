// LSXTransportLib: 数据传输工具库（跨平台）
// 命名空间：LSX_LIB

// ---------- File: TcpServer.cpp ----------
#include "TcpServer.h"
#include <cstring> // 包含 memset
#include <limits> // 包含 numeric_limits
#include <algorithm> // 包含 std::min
#ifdef _WIN32
    #include <Ws2tcpip.h> // For inet_pton (already included in h), but also define WIN32_LEAN_AND_MEAN etc.
#else // POSIX
#include <arpa/inet.h> // For inet_ntop (示例中注释掉的代码可能用到)
#endif


namespace LSX_LIB::DataTransfer
{
    TcpServer::TcpServer(uint16_t port)
    {
        std::memset(&localAddr, 0, sizeof(localAddr));
        localAddr.sin_family = AF_INET;
        localAddr.sin_port = htons(port);
        localAddr.sin_addr.s_addr = htonl(INADDR_ANY); // 使用 htonl for INADDR_ANY
    }

    bool TcpServer::create()
    {
        LIBLSX::LockManager::LockGuard<std::mutex> lock(mtx); // 锁定互斥锁

        // 创建监听 socket
#ifdef _WIN32
    listenFd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenFd == INVALID_SOCKET) {
        LIBLSX::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex); // 锁定错误输出
        std::cerr << "TcpServer::create: socket failed. Error: " << WSAGetLastError() << std::endl;
        return false;
    }
#else
        listenFd = socket(AF_INET, SOCK_STREAM, 0);
        if (listenFd < 0)
        {
            LIBLSX::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex); // 锁定错误输出
            std::cerr << "TcpServer::create: socket failed. Error: " << strerror(errno) << std::endl;
            return false;
        }
#endif

        // 允许套接字地址重用
        int opt = 1;
        if (setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt)) < 0)
        {
#ifdef _WIN32
        LIBLSX::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex); // 锁定错误输出
        std::cerr << "TcpServer::create: setsockopt(SO_REUSEADDR) failed. Error: " << WSAGetLastError() << std::endl;
#else
            LIBLSX::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex); // 锁定错误输出
            std::cerr << "TcpServer::create: setsockopt(SO_REUSEADDR) failed. Error: " << strerror(errno) << std::endl;
#endif
            // 不是关键错误，但报告
        }


        // 绑定 socket
        if (bind(listenFd, reinterpret_cast<struct sockaddr*>(&localAddr),
                 sizeof(localAddr)) < 0)
        {
#ifdef _WIN32
        LIBLSX::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex); // 锁定错误输出
        std::cerr << "TcpServer::create: bind failed. Error: " << WSAGetLastError() << std::endl;
        closesocket(listenFd); // 清理套接字
        listenFd = INVALID_SOCKET;
#else
            LIBLSX::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex); // 锁定错误输出
            std::cerr << "TcpServer::create: bind failed. Error: " << strerror(errno) << std::endl;
            ::close(listenFd); // 清理套接字
            listenFd = -1;
#endif
            return false;
        }

        // 监听连接
        if (listen(listenFd, 5) < 0)
        {
            // 5 是待处理连接队列的大小
#ifdef _WIN32
        LIBLSX::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex); // 锁定错误输出
        std::cerr << "TcpServer::create: listen failed. Error: " << WSAGetLastError() << std::endl;
        closesocket(listenFd); // 清理套接字
        listenFd = INVALID_SOCKET;
#else
            LIBLSX::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex); // 锁定错误输出
            std::cerr << "TcpServer::create: listen failed. Error: " << strerror(errno) << std::endl;
            ::close(listenFd); // 清理套接字
            listenFd = -1;
#endif
            return false;
        }

        // connFd 保持无效状态，直到调用 acceptConnection
        return true;
    }

    bool TcpServer::acceptConnection()
    {
        LIBLSX::LockManager::LockGuard<std::mutex> lock(mtx); // 锁定互斥锁

        if (listenFd < 0
#ifdef _WIN32
        || listenFd == INVALID_SOCKET
#endif
        )
        {
            LIBLSX::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex); // 锁定错误输出
            std::cerr << "TcpServer::acceptConnection: Listen socket not created or closed." << std::endl;
            return false;
        }

        // 在接受新连接前关闭任何现有连接 (单客户端行为)
        closeClientConnection(); // closeClientConnection 内部已加锁，此处不会死锁

        struct sockaddr_in clientAddr;
        socklen_t len = sizeof(clientAddr);

        // 接受连接 - 此调用将阻塞直到一个客户端连接，或者监听 socket 超时（如果设置了超时）
        connFd = accept(listenFd,
                        reinterpret_cast<struct sockaddr*>(&clientAddr), &len);

        if (connFd < 0
#ifdef _WIN32
        || connFd == INVALID_SOCKET
#endif
        )
        {
#ifdef _WIN32
        int err = WSAGetLastError();
        // 处理非阻塞或超时 accept 的情况
        if (err == WSAEWOULDBLOCK) {
            // LIBLSX::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex);
            // std::cerr << "TcpServer::acceptConnection: accept would block." << std::endl;
            return false; // 没有待处理连接，返回 false
        }
        if (err == WSAETIMEDOUT) {
            // LIBLSX::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex);
            // std::cerr << "TcpServer::acceptConnection: accept timed out." << std::endl;
             return false; // accept 超时
        }
        LIBLSX::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex); // 锁定错误输出
        std::cerr << "TcpServer::acceptConnection: accept failed. Error: " << err << std::endl;
#else
            // 在 POSIX 上，如果在监听 socket 上设置了 SO_RCVTIMEO，accept 也会超时并返回 -1，errno 为 EAGAIN 或 EWOULDBLOCK。
            // 即使没有设置超时，如果监听 socket 被设置为非阻塞，也会立即返回 -1 并设置 errno 为 EAGAIN/EWOULDBLOCK。
            int err = errno; // 保存 errno
            // 处理非阻塞或超时 accept 的情况
            if (err == EAGAIN || err == EWOULDBLOCK)
            {
                // LIBLSX::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex);
                // std::cerr << "TcpServer::acceptConnection: accept would block/EAGAIN." << std::endl;
                return false; // 没有待处理连接，返回 false
            }
            LIBLSX::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex); // 锁定错误输出
            std::cerr << "TcpServer::acceptConnection: accept failed. Error: " << strerror(err) << std::endl;
#endif
            return false;
        }

        // 成功接受连接
        // {
        //     std::lock_guard<std::mutex> lock_out(g_error_mutex); // 锁定输出
        //     char client_ip[INET_ADDRSTRLEN];
        //     inet_ntop(AF_INET, &(clientAddr.sin_addr), client_ip, INET_ADDRSTRLEN);
        //     std::cout << "TcpServer: Accepted connection from " << client_ip
        //           << ":" << ntohs(clientAddr.sin_port) << std::endl;
        // }


        return true;
    }


    int TcpServer::receive(uint8_t* buffer, size_t size)
    {
        // 修改了函数签名
        LIBLSX::LockManager::LockGuard<std::mutex> lock(mtx); // 锁定互斥锁

        if (connFd < 0
#ifdef _WIN32
        || connFd == INVALID_SOCKET
#endif
        )
        {
            // 注意: 在这个单客户端设计中，如果没接受客户端，这是一个错误。
            // 用户必须先调用 acceptConnection()。
            LIBLSX::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex); // 锁定错误输出
            std::cerr << "TcpServer::receive: No client connection accepted." << std::endl;
            return -1; // 指示错误
        }

        // TCP 接收读取当前可用的数据，最多读取 size 字节到缓冲区。
        // 不保证读取到一个完整的“消息”。
        // 返回值约定：>0 成功（读取字节数），0 连接关闭，<0 错误。
        // 将 size 转换为 int，如果 size 过大可能溢出，此处假定 size 在 int 范围内
        int intSize = static_cast<int>(size);
        if (intSize < 0 || static_cast<size_t>(intSize) != size)
        {
            LIBLSX::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex);
            std::cerr << "TcpServer::receive: Buffer size too large for recv chunk size." << std::endl;
            return -1;
        }

        int received = ::recv(connFd, reinterpret_cast<char*>(buffer),
                              intSize, 0);

        if (received < 0)
        {
#ifdef _WIN32
        int err = WSAGetLastError();
        // 处理超时/非阻塞错误
        if (err == WSAEWOULDBLOCK) {
            // LIBLSX::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex);
            // std::cerr << "TcpServer::receive: recv would block." << std::endl; // 非阻塞模式下当前无数据
            return 0; // 将无可读数据视为读取 0 字节
        }
        if (err == WSAETIMEDOUT) {
            // LIBLSX::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex);
            // std::cerr << "TcpServer::receive: Timed out." << std::endl; // 超时可能不是错误
            return 0; // 将超时视为读取 0 字节
        }
        {
            LIBLSX::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex); // 锁定错误输出
            std::cerr << "TcpServer::receive: recv failed. Error: " << err << std::endl;
        }
#else
            // 处理超时/非阻塞错误
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                // LIBLSX::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex);
                // std::cerr << "TcpServer::receive: recv would block/EAGAIN." << std::endl; // 非阻塞模式下当前无数据
                return 0; // 将无可读数据视为读取 0 字节
            }
            {
                LIBLSX::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex); // 锁定错误输出
                std::cerr << "TcpServer::receive: recv failed. Error: " << strerror(errno) << std::endl;
            }
#endif
            return -1; // 指示错误
        }
        // received == 0 表示连接被对端优雅关闭。
        return received; // 返回读取到的字节数 (>= 0)
    }

    bool TcpServer::send(const uint8_t* data, size_t size)
    {
        LIBLSX::LockManager::LockGuard<std::mutex> lock(mtx); // 锁定互斥锁

        if (connFd < 0
#ifdef _WIN32
       || connFd == INVALID_SOCKET
#endif
        )
        {
            LIBLSX::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex); // 锁定错误输出
            std::cerr << "TcpServer::send: No client connection accepted." << std::endl;
            return false;
        }

        size_t totalSent = 0;
        // 将 size 转换为 int，如果 size 过大可能溢出，此处假定 size 在 int 范围内
        int intSize = static_cast<int>(size);
        if (intSize < 0 || static_cast<size_t>(intSize) != size)
        {
            LIBLSX::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex);
            std::cerr << "TcpServer::send: Data size too large for single send call." << std::endl;
            if (size > static_cast<size_t>(std::numeric_limits<int>::max()))
            {
                std::lock_guard<std::mutex> lock_err_large(g_error_mutex);
                std::cerr << "TcpServer::send: Total data size exceeds maximum send chunk size." << std::endl;
                return false; // 数据过大，需要更高级的分块发送逻辑
            }
            intSize = static_cast<int>(size); // 重新确保 intSize 正确
        }

        while (totalSent < size)
        {
            size_t remaining = size - totalSent;
            int sendSize = static_cast<int>(std::min(remaining, static_cast<size_t>(std::numeric_limits<int>::max())));
            // 确保不超过 int 最大值

            int sent = ::send(connFd, reinterpret_cast<const char*>(data + totalSent),
                              sendSize, 0);

            if (sent < 0)
            {
#ifdef _WIN32
            int err = WSAGetLastError();
            // 处理非阻塞/超时错误
            if (err == WSAEWOULDBLOCK) continue; // 理论上不应该发生
            if (err == WSAETIMEDOUT) {
                LIBLSX::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex); // 锁定错误输出
                std::cerr << "TcpServer::send: Timed out." << std::endl;
                return false; // 发送超时
            }
            {
                LIBLSX::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex); // 锁定错误输出
                std::cerr << "TcpServer::send: send failed. Error: " << err << std::endl;
            }
#else
                // 处理非阻塞/超时错误
                if (errno == EAGAIN || errno == EWOULDBLOCK) continue; // 理论上不应该发生
                {
                    LIBLSX::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex); // 锁定错误输出
                    std::cerr << "TcpServer::send: send failed. Error: " << strerror(errno) << std::endl;
                }
#endif
                return false; // 发生错误
            }
            if (sent == 0)
            {
                // 发送时连接被对端关闭
                LIBLSX::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex); // 锁定错误输出
                std::cerr << "TcpServer::send: Connection closed by peer during send." << std::endl;
                return false;
            }
            totalSent += sent;
        }
        return true; // 所有数据发送完毕
    }

    void TcpServer::closeClientConnection()
    {
        LIBLSX::LockManager::LockGuard<std::mutex> lock(mtx); // 锁定互斥锁

        if (connFd >= 0
#ifdef _WIN32
        || connFd != INVALID_SOCKET
#endif
        )
        {
#ifdef _WIN32
        // 先关闭发送和接收 (可选，但好的实践)
        shutdown(connFd, SD_BOTH); // 忽略错误
        closesocket(connFd);
        connFd = INVALID_SOCKET; // 设置句柄为无效状态
#else
            shutdown(connFd, SHUT_RDWR); // 忽略错误
            ::close(connFd);
            connFd = -1; // 设置句柄为无效状态
#endif
            // {
            //     std::lock_guard<std::mutex> lock_out(g_error_mutex); // 锁定输出
            //     std::cout << "TcpServer: Client connection closed." << std::endl;
            // }
        }
    }

    void TcpServer::close()
    {
        LIBLSX::LockManager::LockGuard<std::mutex> lock(mtx); // 锁定互斥锁

        closeClientConnection(); // 先关闭已接受的连接 (closeClientConnection 内部已加锁，此处不会死锁)

        if (listenFd >= 0
#ifdef _WIN32
        || listenFd != INVALID_SOCKET
#endif
        )
        {
#ifdef _WIN32
        closesocket(listenFd);
        listenFd = INVALID_SOCKET; // 设置句柄为无效状态
#else
            ::close(listenFd);
            listenFd = -1; // 设置句柄为无效状态
#endif
            // WSACleanup 由 WinsockManager 处理
            // {
            //     std::lock_guard<std::mutex> lock_out(g_error_mutex); // 锁定输出
            //     std::cout << "TcpServer: Listen socket closed." << std::endl;
            // }
        }
    }


    TcpServer::~TcpServer()
    {
        close(); // 确保在析构时调用 close
    }

    bool TcpServer::setSendTimeout(int timeout_ms)
    {
        LIBLSX::LockManager::LockGuard<std::mutex> lock(mtx); // 锁定互斥锁

        // 将超时应用于已接受的连接 socket
        if (connFd < 0
#ifdef _WIN32
        || connFd == INVALID_SOCKET
#endif
        )
        {
            LIBLSX::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex); // 锁定错误输出
            std::cerr << "TcpServer::setSendTimeout: No client connection accepted." << std::endl;
            return false; // 只能在连接的 socket 上设置超时
        }
#ifdef _WIN32
    DWORD timeout = timeout_ms;
    if (setsockopt(connFd, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout)) == SOCKET_ERROR) {
        LIBLSX::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex); // 锁定错误输出
        std::cerr << "TcpServer::setSendTimeout failed. Error: " << WSAGetLastError() << std::endl;
        return false;
    }
#else
        struct timeval tv;
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        if (setsockopt(connFd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0)
        {
            LIBLSX::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex); // 锁定错误输出
            std::cerr << "TcpServer::setSendTimeout failed. Error: " << strerror(errno) << std::endl;
            return false;
        }
#endif
        return true;
    }

    bool TcpServer::setReceiveTimeout(int timeout_ms)
    {
        LIBLSX::LockManager::LockGuard<std::mutex> lock(mtx); // 锁定互斥锁

        // 将超时应用于已接受的连接 socket
        if (connFd < 0
#ifdef _WIN32
        || connFd == INVALID_SOCKET
#endif
        )
        {
            LIBLSX::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex); // 锁定错误输出
            std::cerr << "TcpServer::setReceiveTimeout: No client connection accepted." << std::endl;
            return false; // 只能在连接的 socket 上设置超时
        }
#ifdef _WIN32
    DWORD timeout = timeout_ms;
    if (setsockopt(connFd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout)) == SOCKET_ERROR) {
        LIBLSX::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex); // 锁定错误输出
        std::cerr << "TcpServer::setReceiveTimeout failed. Error: " << WSAGetLastError() << std::endl;
        return false;
    }
#else
        struct timeval tv;
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        if (setsockopt(connFd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
        {
            LIBLSX::LockManager::LockGuard<std::mutex> lock_err(g_error_mutex); // 锁定错误输出
            std::cerr << "TcpServer::setReceiveTimeout failed. Error: " << strerror(errno) << std::endl;
            return false;
        }
#endif
        return true;
    }
} // namespace LSX_LIB
