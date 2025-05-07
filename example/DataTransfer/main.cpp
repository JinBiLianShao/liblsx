// LSXTransportLib: 数据传输工具库（跨平台）
// 命名空间： LSX_LIB::DataTransfer

// ---------- File: main.cpp (示例) ----------
#include <iostream>
#include <vector> // 用于将接收数据存入 vector
#include <thread> // 用于多线程示例 (可选)
#include <chrono> // 用于计时或延时 (可选)
#include "CommunicationFactory.h"
#include "TcpServer.h" // 包含 TcpServer 头文件以便使用 acceptConnection

// 包含 Winsock Manager 和全局错误锁的 cpp 文件，以确保其静态实例在主编译单元中定义和链接
// 在实际项目中，这些通常作为库的一部分编译，用户只需要链接库即可。
// #include "WinsockManager.cpp"
// #include "GlobalErrorMutex.cpp"

// 使用锁保护全局输出流 (std::cout, std::cerr)
#define PROTECT_COUT(code) do { std::lock_guard<std::mutex> lock( LSX_LIB::DataTransfer::g_error_mutex); code; } while(0)


// UDP 客户端测试函数 (可在单独线程中运行)
void udp_client_test(const std::string& ip, uint16_t port, int timeout_ms)
{
    PROTECT_COUT(std::cout << "--- UDP Client Test (" << ip << ":" << port << ") ---" << std::endl;);
    auto comm = LSX_LIB::DataTransfer::CommunicationFactory::create(LSX_LIB::DataTransfer::CommType::UDP_CLIENT, ip,
                                                                    port, "", 0, timeout_ms);
    if (!comm || !comm->create())
    {
        PROTECT_COUT(std::cerr << "Failed to create/connect UDP client." << std::endl;);
        return;
    }

    const char* msg = "Hello UDP Server!";
    if (comm->send(reinterpret_cast<const uint8_t*>(msg), strlen(msg)))
    {
        PROTECT_COUT(std::cout << "UDP Client sent: " << msg << std::endl;);
    }
    else
    {
        PROTECT_COUT(std::cerr << "UDP Client send failed." << std::endl;);
    }

    std::vector<uint8_t> buf(128);
    // UDP receive 获取一个数据报，最多 buffer.size() 字节。
    // 返回值: -1 错误, >=0 接收到的字节数。
    int len = comm->receive(buf.data(), buf.size());
    if (len > 0)
    {
        PROTECT_COUT(
            std::cout << "UDP Client received: " << std::string(reinterpret_cast<char*>(buf.data()), len) << std::endl;)
        ;
    }
    else if (len == 0)
    {
        PROTECT_COUT(std::cout << "UDP Client receive timed out or no data received." << std::endl;);
    }
    else
    {
        // len < 0
        PROTECT_COUT(std::cerr << "UDP Client receive failed." << std::endl;);
    }
    comm->close();
    PROTECT_COUT(std::cout << "UDP Client test finished." << std::endl;);
}

// TCP 客户端测试函数 (可在单独线程中运行)
void tcp_client_test(const std::string& ip, uint16_t port, int timeout_ms)
{
    PROTECT_COUT(std::cout << "--- TCP Client Test (" << ip << ":" << port << ") ---" << std::endl;);
    // 注意: 你需要一个 TCP 服务器运行在指定的 IP 和端口上才能测试。
    auto comm = LSX_LIB::DataTransfer::CommunicationFactory::create(LSX_LIB::DataTransfer::CommType::TCP_CLIENT, ip,
                                                                    port, "", 0, timeout_ms);
    if (!comm || !comm->create())
    {
        PROTECT_COUT(std::cerr << "Failed to create/connect TCP client." << std::endl;);
        return;
    }

    const char* msg = "Hello TCP Server!";
    if (comm->send(reinterpret_cast<const uint8_t*>(msg), strlen(msg)))
    {
        PROTECT_COUT(std::cout << "TCP Client sent: " << msg << std::endl;);
    }
    else
    {
        PROTECT_COUT(std::cerr << "TCP Client send failed." << std::endl;);
    }

    std::vector<uint8_t> buf(128);
    // TCP receive 可能只收到部分数据。调用者需要实现分帧逻辑。
    // 返回值: >0 读取字节数, 0 连接关闭, <0 错误。
    int len = comm->receive(buf.data(), buf.size());
    if (len > 0)
    {
        PROTECT_COUT(
            std::cout << "TCP Client received: " << std::string(reinterpret_cast<char*>(buf.data()), len) << std::endl;)
        ;
    }
    else if (len == 0)
    {
        PROTECT_COUT(std::cout << "TCP Client receive timed out or connection closed." << std::endl;);
    }
    else
    {
        // len < 0
        PROTECT_COUT(std::cerr << "TCP Client receive failed." << std::endl;);
    }
    comm->close();
    PROTECT_COUT(std::cout << "TCP Client test finished." << std::endl;);
}


// TCP 服务器测试函数 (单客户端) (可在单独线程中运行)
void tcp_server_test(uint16_t port, int timeout_ms)
{
    PROTECT_COUT(std::cout << "--- TCP Server Test (Single Client on port " << port << ") ---" << std::endl;);
    // 注意: 这个服务器监听在指定端口，只接受一个连接。
    // 然后尝试接收和发送。
    auto comm = LSX_LIB::DataTransfer::CommunicationFactory::create(LSX_LIB::DataTransfer::CommType::TCP_SERVER, "",
                                                                    port, "", 0, timeout_ms);
    if (!comm || !comm->create())
    {
        PROTECT_COUT(std::cerr << "Failed to create TCP server (listen)." << std::endl;);
        return;
    }

    PROTECT_COUT(std::cout << "TCP Server listening on port " << port << ". Waiting for a connection..." << std::endl;);
    // 将通用指针转换为 TcpServer*，以便访问 acceptConnection 方法
    auto* tcp_server_ptr = dynamic_cast<LSX_LIB::DataTransfer::TcpServer*>(comm.get());
    if (tcp_server_ptr && tcp_server_ptr->acceptConnection())
    {
        // acceptConnection 是阻塞的，直到连接建立或超时
        PROTECT_COUT(std::cout << "TCP Server accepted connection." << std::endl;);
        std::vector<uint8_t> buf(128);
        // 在已接受的连接上使用 receive
        int len = tcp_server_ptr->receive(buf.data(), buf.size());
        if (len > 0)
        {
            PROTECT_COUT(
                std::cout << "TCP Server received: " << std::string(reinterpret_cast<char*>(buf.data()), len) << std::
                endl;);
            const char* response = "OK from TCP Server";
            // 在已接受的连接上使用 send
            tcp_server_ptr->send(reinterpret_cast<const uint8_t*>(response), strlen(response));
            PROTECT_COUT(std::cout << "TCP Server sent response." << std::endl;);
        }
        else if (len == 0)
        {
            PROTECT_COUT(std::cout << "TCP Server receive timed out or client disconnected." << std::endl;);
        }
        else
        {
            // len < 0
            PROTECT_COUT(std::cerr << "TCP Server receive failed." << std::endl;);
        }
        // 如果需要，显式关闭客户端连接
        tcp_server_ptr->closeClientConnection();
        PROTECT_COUT(std::cout << "TCP Server client connection closed." << std::endl;);
    }
    else
    {
        PROTECT_COUT(std::cerr << "TCP Server failed to accept connection (timeout or error)." << std::endl;);
    }
    comm->close(); // 关闭监听 socket
    PROTECT_COUT(std::cout << "TCP Server test finished." << std::endl;);
}


// 串口测试函数 (可在单独线程中运行)
void serial_test(const std::string& port_name, int baud_rate, int timeout_ms)
{
    PROTECT_COUT(std::cout << "--- Serial Port Test (" << port_name << " @ " << baud_rate << ") ---" << std::endl;);
    // 注意: 替换串口名称为你实际的设备名称。
    // 确保有设备连接并在指定的波特率下发送/接收数据。
    // 这个示例使用超时，所以不会无限期阻塞。

    auto comm = LSX_LIB::DataTransfer::CommunicationFactory::create(LSX_LIB::DataTransfer::CommType::SERIAL, "", 0,
                                                                    port_name, baud_rate,
                                                                    timeout_ms);
    if (!comm || !comm->create())
    {
        PROTECT_COUT(std::cerr << "Failed to create/open serial port: " << port_name << std::endl;);
        return;
    }
    else
    {
        PROTECT_COUT(std::cout << "Serial Port opened: " << port_name << " at " << baud_rate << " baud." << std::endl;);

        // 可选: 发送一个消息
        const char* serial_msg = "Hello Serial!";
        if (comm->send(reinterpret_cast<const uint8_t*>(serial_msg), strlen(serial_msg)))
        {
            PROTECT_COUT(std::cout << "Serial sent: " << serial_msg << std::endl;);
        }
        else
        {
            PROTECT_COUT(std::cerr << "Serial send failed." << std::endl;);
        }

        // 尝试接收数据
        std::vector<uint8_t> buf(128);
        // 串口 receive 读取当前可用数据，最多 buffer.size() 字节，遵守超时配置。
        // 返回值: >0 读取字节数, 0 超时/无数据, <0 错误。
        int len = comm->receive(buf.data(), buf.size());
        if (len > 0)
        {
            PROTECT_COUT(
                std::cout << "Serial received " << len << " bytes: " << std::string(reinterpret_cast<char*>(buf.data()),
                    len) << std::endl;);
        }
        else if (len == 0)
        {
            PROTECT_COUT(std::cout << "Serial receive timed out or no data available." << std::endl;);
        }
        else
        {
            // len < 0
            PROTECT_COUT(std::cerr << "Serial receive failed." << std::endl;);
        }

        comm->close();
        PROTECT_COUT(std::cout << "Serial Port test finished." << std::endl;);
    }
}


int main()
{
    using namespace LSX_LIB::DataTransfer;

    // 注意: 如果要在同一个通信对象上进行多线程操作 (例如，一个线程发送，另一个线程接收，
    // 使用同一个 TcpClient 或 SerialPort 对象)，库内部的 Mutex 会提供保护。
    // 但是，更常见的多线程网络应用模式是每个客户端连接或每个任务在自己的通信对象上操作。
    // TcpServer 示例是单连接的，如果需要并发处理多个客户端，需要重新设计。

    // 运行示例测试 (可以在不同线程中运行以测试库的线程安全 - 对象内部)

    // UDP 客户端测试 (连接到本地回环 127.0.0.1:9000)
    // 需要一个 UDP 服务器运行在 127.0.0.1:9000
    // std::thread udp_client_thread(udp_client_test, "127.0.0.1", 9000, 1000); // 1秒超时

    // TCP 客户端测试 (连接到本地回环 127.0.0.1:9001)
    // 需要一个 TCP 服务器运行在 127.0.0.1:9001
    // std::thread tcp_client_thread(tcp_client_test, "127.0.0.1", 9001, 1000); // 1秒超时

    // TCP 服务器测试 (监听端口 9002，单客户端)
    // std::thread tcp_server_thread(tcp_server_test, 9002, 5000); // accept 等待 5秒超时

    // 串口测试 (替换为你的串口名称和波特率)
#ifdef _WIN32
    const std::string serial_port_name = "COM3"; // 更改为你的 Windows COM 端口
#else
    const std::string serial_port_name = "/dev/ttyUSB0"; // 更改为你的 Linux/macOS 串口
#endif
    int serial_baud_rate = 9600;
    // std::thread serial_thread(serial_test, serial_port_name, serial_baud_rate, 1000); // 1秒超时

    // 如果创建了线程，等待它们完成
    // if(udp_client_thread.joinable()) udp_client_thread.join();
    // if(tcp_client_thread.joinable()) tcp_client_thread.join();
    // if(tcp_server_thread.joinable()) tcp_server_thread.join();
    // if(serial_thread.joinable()) serial_thread.join();


    // 或者直接顺序运行测试 (单线程)
    udp_client_test("127.0.0.1", 9000, 1000);
    tcp_client_test("127.0.0.1", 9001, 1000);
    tcp_server_test(9002, 5000);
    serial_test(serial_port_name, serial_baud_rate, 1000); // 串口测试请根据需要启用和修改参数

    return 0;
}
