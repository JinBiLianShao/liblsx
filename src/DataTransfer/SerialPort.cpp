// LSXTransportLib: 数据传输工具库（跨平台）
// 命名空间：LSX_LIB

// ---------- File: SerialPort.cpp ----------
#include "SerialPort.h"
#include <iostream> // 包含 std::cerr, std::cout
#include <cstring> // 包含 memset
#include <limits> // 包含 numeric_limits
#ifdef _WIN32
#include <algorithm> // For std::min
#else
#include <algorithm> // For std::min
#endif

namespace LSX_LIB::DataTransfer
{
    SerialPort::SerialPort(const std::string& port, int baud)
        : portName(port), baudRate(baud)
    {
    }

    int SerialPort::getPlatformBaudRate(int baudRate)
    {
#ifdef _WIN32
        switch (baudRate)
        {
        case 0: return 0; // 使用 DCB 中的默认值
        case 110: return CBR_110;
        case 300: return CBR_300;
        case 600: return CBR_600;
        case 1200: return CBR_1200;
        case 2400: return CBR_2400;
        case 4800: return CBR_4800;
        case 9600: return CBR_9600;
        case 14400: return CBR_14400;
        case 19200: return CBR_19200;
        case 38400: return CBR_38400;
        case 57600: return CBR_57600;
        case 115200: return CBR_115200;
        case 128000: return CBR_128000;
        case 256000: return CBR_256000;
        // 注意：更高波特率可能需要直接使用数值
        // case 230400: return 230400; // CBR_230400 在某些头文件中有定义
        // case 460800: return 460800;
        default:
            {
                LIBLSX::LockManager::LockGuard<std::mutex> lock(g_error_mutex); // 锁定错误输出
                std::cerr << "SerialPort::getPlatformBaudRate: Unsupported baud rate on Windows: " << baudRate <<
                    std::endl;
            }
            return -1; // 指示不支持的波特率
        }
#else // POSIX
        switch (baudRate)
        {
        case 0: return B0; // 忽略波特率设置
        case 50: return B50;
        case 75: return B75;
        case 110: return B110;
        case 134: return B134;
        case 150: return B150;
        case 200: return B200;
        case 300: return B300;
        case 600: return B600;
        case 1200: return B1200;
        case 1800: return B1800;
        case 2400: return B2400;
        case 4800: return B4800;
        case 9600: return B9600;
        case 19200: return B19200;
        case 38400: return B38400;
        case 57600: return B57600;
        case 115200: return B115200;
        case 230400: return B230400;
        case 460800: return B460800;
        case 500000: return B500000;
        case 576000: return B576000;
        case 921600: return B921600;
        case 1000000: return B1000000;
        case 1152000: return B1152000;
        case 1500000: return B1500000;
        case 2000000: return B2000000;
        case 2500000: return B2500000;
        case 3000000: return B3000000;
        case 3500000: return B3500000;
        case 4000000: return B4000000;
        default:
            {
                LIBLSX::LockManager::LockGuard<std::mutex> lock(g_error_mutex); // 锁定错误输出
                std::cerr << "SerialPort::getPlatformBaudRate: Unsupported baud rate on POSIX: " << baudRate <<
                    std::endl;
            }
            return -1; // 指示不支持的波特率
        }
#endif
    }

    bool SerialPort::create()
    {
        LIBLSX::LockManager::LockGuard<std::mutex> lock(mtx); // 锁定互斥锁

#ifdef _WIN32
        // Windows 实现
        // 对于 COM 端口号 > 9 的，需要添加 "\\\\.\\" 前缀
        std::string fullPortName = portName;
        if (portName.size() > 3 && (portName.substr(0, 3) == "COM" || portName.substr(0, 3) == "com"))
        {
            try
            {
                int portNum = std::stoi(portName.substr(3));
                if (portNum > 9)
                {
                    fullPortName = "\\\\.\\" + portName;
                }
            }
            catch (const std::invalid_argument& ia)
            {
                // 端口名不是 COMx 格式，忽略 stoi 错误
                // LIBLSX::LockManager::LockGuard<std::mutex> lock(g_error_mutex);
                // std::cerr << "SerialPort::create: Port name '" << portName << "' is not in COMx format. " << ia.what() << std::endl;
            } catch (const std::out_of_range& oor)
            {
                // 端口号过大
                LIBLSX::LockManager::LockGuard<std::mutex> lock(g_error_mutex);
                std::cerr << "SerialPort::create: Port number out of range for '" << portName << "'. " << oor.what() <<
                    std::endl;
                return false;
            }
        }


        hComm = CreateFileA(fullPortName.c_str(), // 端口名称
                            GENERIC_READ | GENERIC_WRITE, // 期望的访问权限
                            0, // 共享模式
                            NULL, // 安全属性
                            OPEN_EXISTING, // 创建方式：打开现有设备
                            0, // FILE_ATTRIBUTE_NORMAL,    // 标志和属性 (0 for 同步阻塞 I/O)
                            NULL); // 模板文件句柄 (串口不需要)

        if (hComm == INVALID_HANDLE_VALUE)
        {
            LIBLSX::LockManager::LockGuard<std::mutex> lock(g_error_mutex); // 锁定错误输出
            std::cerr << "SerialPort::create: CreateFile failed for " << portName << ". Error: " << GetLastError() <<
                std::endl;
            return false;
        }

        DCB dcbSerialParams = {0}; // 设备控制块结构
        dcbSerialParams.DCBlength = sizeof(dcbSerialParams);

        if (!GetCommState(hComm, &dcbSerialParams))
        {
            LIBLSX::LockManager::LockGuard<std::mutex> lock(g_error_mutex); // 锁定错误输出
            std::cerr << "SerialPort::create: GetCommState failed. Error: " << GetLastError() << std::endl;
            CloseHandle(hComm);
            hComm = INVALID_HANDLE_VALUE;
            return false;
        }

        int platformBaud = getPlatformBaudRate(baudRate);
        if (platformBaud == -1)
        {
            // getPlatformBaudRate 中已报告错误
            CloseHandle(hComm);
            hComm = INVALID_HANDLE_VALUE;
            return false;
        }
        dcbSerialParams.BaudRate = platformBaud; // 波特率
        dcbSerialParams.ByteSize = 8; // 8 数据位
        dcbSerialParams.StopBits = ONESTOPBIT; // 1 停止位
        dcbSerialParams.Parity = NOPARITY; // 无校验位

        if (!SetCommState(hComm, &dcbSerialParams))
        {
            LIBLSX::LockManager::LockGuard<std::mutex> lock(g_error_mutex); // 锁定错误输出
            std::cerr << "SerialPort::create: SetCommState failed. Error: " << GetLastError() << std::endl;
            CloseHandle(hComm);
            hComm = INVALID_HANDLE_VALUE;
            return false;
        }

        // 设置超时 (Windows COMMTIMEOUTS)
        // ReadIntervalTimeout: 两个字符之间的最大时间间隔。如果超过，读操作完成。
        // ReadTotalTimeoutMultiplier: 读取的每个字节的最大时间（乘以请求字节数）。
        // ReadTotalTimeoutConstant: 读操作的总超时常数。
        // ReadTotalTimeout = (ReadTotalTimeoutMultiplier * BytesToRead) + ReadTotalTimeoutConstant
        // WriteTotalTimeout = (WriteTotalTimeoutMultiplier * BytesToWrite) + WriteTotalTimeoutConstant
        //
        // 常见配置:
        // ReadIntervalTimeout=MAXDWORD, ReadTotalTimeoutConstant=C, ReadTotalTimeoutMultiplier=M: 读 C + M * bytes 后完成。
        // ReadIntervalTimeout=MAXDWORD, ReadTotalTimeoutConstant=0, ReadTotalTimeoutMultiplier=0: 读阻塞直到请求字节数，无超时。
        // ReadIntervalTimeout=0, ReadTotalTimeoutConstant=C, ReadTotalTimeoutMultiplier=0: 读最多 C ms，返回所有可用字节。
        // ReadIntervalTimeout=0, ReadTotalTimeoutConstant=0, ReadTotalTimeoutMultiplier=0: 非阻塞，立即返回可用字节。
        // ReadIntervalTimeout=C, ReadTotalTimeoutConstant=0, ReadTotalTimeoutMultiplier=0: 读等待第一个字节 C ms，之后非阻塞。
        // ReadIntervalTimeout=C, ReadTotalTimeoutConstant=M, ReadTotalTimeoutMultiplier=0: 读等待第一个字节 M ms，之后每字符 C ms。

        // 为了实现类似 ICommunication 的 receive 超时 (等待指定时间，返回所有可用)，使用 ReadIntervalTimeout = 0, ReadTotalTimeoutConstant = timeout_ms, ReadTotalTimeoutMultiplier = 0
        // 但是 CreateFile 使用 0 标志打开是阻塞模式，ReadFile 会阻塞直到有数据或错误。
        // 使用 ReadFile 时，如果设置 ReadIntervalTimeout 和 ReadTotalTimeoutConstant/Multiplier，
        // 它会在超时时返回 FALSE，且 GetLastError() 为 WAIT_TIMEOUT。
        // 对于阻塞模式，简单的做法是设置 ReadTotalTimeoutConstant 为超时时间，Multiplier 为 0。
        // ReadIntervalTimeout 可以设置为 0 或一个较小值。
        COMMTIMEOUTS timeouts = {0};
        // 设置一个简单的总读取超时
        timeouts.ReadTotalTimeoutConstant = 100; // 示例: 100ms 总读取超时
        timeouts.ReadTotalTimeoutMultiplier = 0; // 无乘数
        // 设置一个简单的总写入超时
        timeouts.WriteTotalTimeoutConstant = 100; // 示例: 100ms 总写入超时
        timeouts.WriteTotalTimeoutMultiplier = 0; // 无乘数
        // 如果需要 interval timeout，可以设置 ReadIntervalTimeout

        if (!SetCommTimeouts(hComm, &timeouts))
        {
            LIBLSX::LockManager::LockGuard<std::mutex> lock(g_error_mutex); // 锁定错误输出
            std::cerr << "SerialPort::create: SetCommTimeouts failed. Error: " << GetLastError() << std::endl;
            // 不一定是致命错误，但报告
        }

        // 清空缓冲区 (可选)
        PurgeComm(hComm, PURGE_RXCLEAR | PURGE_TXCLEAR);


        return true;
#else // POSIX 实现
        // 以阻塞模式打开串口
        fd = open(portName.c_str(), O_RDWR | O_NOCTTY); // 移除 O_NDELAY 实现阻塞模式
        if (fd < 0)
        {
            LIBLSX::LockManager::LockGuard<std::mutex> lock(g_error_mutex); // 锁定错误输出
            std::cerr << "SerialPort::create: open failed for " << portName << ". Error: " << strerror(errno) <<
                std::endl;
            return false;
        }

        // 获取当前设置，以便关闭时恢复
        if (tcgetattr(fd, &oldtio) < 0)
        {
            LIBLSX::LockManager::LockGuard<std::mutex> lock(g_error_mutex); // 锁定错误输出
            std::cerr << "SerialPort::create: tcgetattr failed. Error: " << strerror(errno) << std::endl;
            ::close(fd);
            fd = -1;
            return false;
        }

        struct termios newtio;
        std::memset(&newtio, 0, sizeof(newtio)); // 清零结构体

        // 配置设置 (8N1, 启用接收器, 忽略调制解调器状态线)
        newtio.c_cflag = getPlatformBaudRate(baudRate) | CS8 | CLOCAL | CREAD;
        newtio.c_iflag = IGNPAR; // 忽略奇偶校验错误的字节
        newtio.c_oflag = 0; // 无输出处理 (原始模式)
        newtio.c_lflag = 0; // 无行处理 (原始模式)

        // 设置 VTIME 和 VMIN 控制读取行为 (阻塞模式)
        // VTIME = 0, VMIN > 0: read 阻塞直到 VMIN 字节被接收。
        // VTIME > 0, VMIN = 0: read 阻塞最多 VTIME * 0.1 秒，返回收到的任何字节 (或 0 如果超时)。
        // VTIME > 0, VMIN > 0: read 阻塞直到 VMIN 字节 或 第一个字节后 VTIME * 0.1 秒。
        // VTIME = 0, VMIN = 0: 非阻塞 read (立即返回可用字节，可能为 0)。

        // 为了实现类似 ICommunication 的 receive 超时，使用 VTIME > 0, VMIN = 0。
        // VTIME 单位是 0.1 秒，所以 timeout_ms / 100 要转换为 VTIME。
        // 注意：VTIME 只能是 0-255。更长的超时需要多次 read 或其他机制。
        // 这里的实现使用固定的 VTIME，以便 receive 可以返回 0 表示无数据（在阻塞模式下模拟超时）
        newtio.c_cc[VTIME] = 1; // 等待 0.1 秒 (如果 VMIN=0)
        newtio.c_cc[VMIN] = 0; // 返回接收到的任何字节 (如果 VTIME > 0)

        // 清除线路上的数据并激活设置
        tcflush(fd, TCIFLUSH); // 丢弃输入缓冲区中的数据
        if (tcsetattr(fd, TCSANOW, &newtio) < 0)
        {
            LIBLSX::LockManager::LockGuard<std::mutex> lock(g_error_mutex); // 锁定错误输出
            std::cerr << "SerialPort::create: tcsetattr failed. Error: " << strerror(errno) << std::endl;
            ::close(fd);
            fd = -1;
            return false;
        }

        return true;
#endif
    }

    bool SerialPort::send(const uint8_t* data, size_t size)
    {
        LIBLSX::LockManager::LockGuard<std::mutex> lock(mtx); // 锁定互斥锁

#ifdef _WIN32
        if (hComm == INVALID_HANDLE_VALUE)
        {
            LIBLSX::LockManager::LockGuard<std::mutex> lock(g_error_mutex); // 锁定错误输出
            std::cerr << "SerialPort::send: Port not created or closed." << std::endl;
            return false;
        }

        DWORD totalWritten = 0;
        // WriteFile 一次写入的最大字节数是 DWORD 的最大值，但对于串口通常远小于此
        // 这里的循环确保所有字节都被尝试写入
        size_t totalSent = 0;
        while (totalSent < size)
        {
            size_t remaining = size - totalSent;
            DWORD bytesToWrite = static_cast<DWORD>(std::min(
                remaining, static_cast<size_t>(std::numeric_limits<DWORD>::max()))); // 确保不超过 DWORD 最大值

            DWORD bytesWritten = 0;
            bool ok = WriteFile(hComm, // 串口句柄
                                data + totalSent, // 数据缓冲区偏移
                                bytesToWrite, // 本次写入的字节数
                                &bytesWritten, // 实际写入的字节数
                                NULL); // Overlapped (同步 I/O 使用 NULL)

            if (!ok)
            {
                // 检查 GetLastError 获取详细信息
                DWORD err = GetLastError();
                // 处理 WriteFile 的超时错误
                if (err == WAIT_TIMEOUT)
                {
                    // LIBLSX::LockManager::LockGuard<std::mutex> lock(g_error_mutex);
                    // std::cerr << "SerialPort::send: WriteFile timed out." << std::endl;
                    // 超时后继续尝试发送剩余数据，除非总超时由 COMMTIMEOUTS 控制
                    // 这里根据 COMMTIMEOUTS 的 WriteTotalTimeout 来判断是否总超时
                    continue; // 理论上 WriteFile 在阻塞模式+TotalTimeout 下超时会返回 FALSE，此处可能需要更精细处理
                }
                LIBLSX::LockManager::LockGuard<std::mutex> lock(g_error_mutex); // 锁定错误输出
                std::cerr << "SerialPort::send: WriteFile failed. Error: " << err << std::endl;
                return false; // 写入失败
            }
            // 如果 WriteFile 成功，bytesWritten 指示实际写入字节数
            if (bytesWritten < bytesToWrite && bytesWritten > 0)
            {
                // 写入不完整，但有部分写入，继续循环
                LIBLSX::LockManager::LockGuard<std::mutex> lock(g_error_mutex); // 锁定错误输出
                std::cerr << "SerialPort::send: Warning - WriteFile incomplete (" << bytesWritten << "/" << bytesToWrite
                    << "), continuing." << std::endl;
            }
            else if (bytesWritten == 0 && bytesToWrite > 0 && ok)
            {
                // 请求写入但实际写入0字节，且无错误（可能由于 timeouts 配置）
                // 在某些 timeout 配置下，WriteFile 可能会返回 TRUE 和 bytesWritten = 0 表示超时或无数据写入
                // 这通常发生在 WriteTotalTimeoutConstant 和 WriteTotalTimeoutMultiplier 都很小或为0时
                LIBLSX::LockManager::LockGuard<std::mutex> lock(g_error_mutex); // 锁定错误输出
                std::cerr << "SerialPort::send: Warning - WriteFile wrote 0 bytes (" << bytesWritten << "/" <<
                    bytesToWrite << "), check timeouts." << std::endl;
                // 这里可能需要根据超时配置决定是重试还是失败
                return false; // 假设写 0 字节意味着出现了问题或无法满足写请求
            }

            totalSent += bytesWritten; // 累计实际写入字节数
        }
        return true; // 所有数据发送完毕
#else // POSIX
        if (fd < 0)
        {
            LIBLSX::LockManager::LockGuard<std::mutex> lock(g_error_mutex); // 锁定错误输出
            std::cerr << "SerialPort::send: Port not created or closed." << std::endl;
            return false;
        }

        size_t totalWritten = 0;
        // write 函数一次写入的最大字节数通常是 int 的最大值，ssize_t 也是
        // 这里的循环确保所有字节都被尝试写入
        while (totalWritten < size)
        {
            size_t remaining = size - totalWritten;
            size_t writeSize = std::min(remaining, static_cast<size_t>(std::numeric_limits<ssize_t>::max()));
            // 确保不超过 ssize_t 最大值

            ssize_t written = ::write(fd, data + totalWritten, writeSize);

            if (written < 0)
            {
                // 处理非阻塞 ( যদিও 我们配置了阻塞模式，但在某些 VTIME/VMIN 配置下 read/write 仍然可能返回 EAGAIN/EWOULDBLOCK) 或其他错误
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    // 如果返回 EAGAIN/EWOULDBLOCK，表示非阻塞模式下当前无法写入
                    // 在 VTIME/VMIN 配置下通常不会发生，除非 VTIME=0, VMIN=0
                    // 如果配置了阻塞模式（VTIME > 0 或 VMIN > 0），write 应该阻塞直到数据写入或发生错误
                    // 这里假设是阻塞模式，EAGAIN/EWOULDBLOCK 是意外情况，或需要更精细的非阻塞处理
                    LIBLSX::LockManager::LockGuard<std::mutex> lock(g_error_mutex); // 锁定错误输出
                    std::cerr << "SerialPort::send: Warning - write returned EAGAIN/EWOULDBLOCK." << std::endl;
                    continue; // 重试
                }
                {
                    LIBLSX::LockManager::LockGuard<std::mutex> lock(g_error_mutex); // 锁定错误输出
                    std::cerr << "SerialPort::send: write failed. Error: " << strerror(errno) << std::endl;
                }
                return false; // 发生错误
            }
            if (written == 0 && writeSize > 0)
            {
                // 请求写入但实际写入0字节，可能是 VTIME/VMIN 配置导致的超时行为
                LIBLSX::LockManager::LockGuard<std::mutex> lock(g_error_mutex); // 锁定错误输出
                std::cerr << "SerialPort::send: Warning - write wrote 0 bytes (" << written << "/" << writeSize <<
                    "), check VTIME/VMIN or timeout." << std::endl;
                // 在这里，写 0 字节可能意味着超时，我们假设是失败
                return false;
            }

            totalWritten += written; // 累计实际写入字节数
        }
        return true; // 所有数据发送完毕
#endif
    }

    int SerialPort::receive(uint8_t* buffer, size_t size)
    {
        // 修改了函数签名
        LIBLSX::LockManager::LockGuard<std::mutex> lock(mtx); // 锁定互斥锁

#ifdef _WIN32
        if (hComm == INVALID_HANDLE_VALUE)
        {
            LIBLSX::LockManager::LockGuard<std::mutex> lock(g_error_mutex); // 锁定错误输出
            std::cerr << "SerialPort::receive: Port not created or closed." << std::endl;
            return -1; // 指示错误
        }

        DWORD bytesRead = 0;
        // ReadFile 一次读取的最大字节数是 DWORD 的最大值，但对于串口通常远小于此
        // 这里读取最多 size 字节，由 COMMTIMEOUTS 控制超时行为
        DWORD bytesToRead = static_cast<DWORD>(std::min(size, static_cast<size_t>(std::numeric_limits<DWORD>::max())));
        // 确保不超过 DWORD 最大值

        bool ok = ReadFile(hComm, // 串口句柄
                           buffer, // 缓冲区
                           bytesToRead, // 本次读取的字节数
                           &bytesRead, // 实际读取的字节数
                           NULL); // Overlapped (同步读取使用 NULL)

        if (!ok)
        {
            DWORD err = GetLastError();
            // 处理 ReadFile 的超时错误 (通常返回 FALSE 和 GetLastError 是 WAIT_TIMEOUT)
            if (err == WAIT_TIMEOUT)
            {
                // LIBLSX::LockManager::LockGuard<std::mutex> lock(g_error_mutex);
                // std::cerr << "SerialPort::receive: ReadFile timed out." << std::endl; // 超时可能不是错误
                return 0; // 将超时视为读取 0 字节
            }
            LIBLSX::LockManager::LockGuard<std::mutex> lock(g_error_mutex); // 锁定错误输出
            std::cerr << "SerialPort::receive: ReadFile failed. Error: " << err << std::endl;
            return -1; // 指示错误
        }

        return static_cast<int>(bytesRead); // 返回实际读取到的字节数 (>= 0)
#else // POSIX
        if (fd < 0)
        {
            LIBLSX::LockManager::LockGuard<std::mutex> lock(g_error_mutex); // 锁定错误输出
            std::cerr << "SerialPort::receive: Port not created or closed." << std::endl;
            return -1; // 指示错误
        }

        // read 返回读取的字节数，0 如果 VTIME 超时且 VMIN=0，或 -1 错误。
        // VTIME=1, VMIN=0 配置下，read 会阻塞最多 0.1 秒。如果在 0.1 秒内收到数据，立即返回已收到字节数。
        // 如果 0.1 秒内没有收到数据，返回 0。
        ssize_t received = ::read(fd, buffer, size);

        if (received < 0)
        {
            // 处理非阻塞 (EAGAIN/EWOULDBLOCK) 或其他错误
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                // LIBLSX::LockManager::LockGuard<std::mutex> lock(g_error_mutex);
                // std::cerr << "SerialPort::receive: read returned EAGAIN/EWOULDBLOCK." << std::endl; // 非阻塞模式下当前无数据
                return 0; // 将无可读数据或超时视为读取 0 字节
            }
            {
                LIBLSX::LockManager::LockGuard<std::mutex> lock(g_error_mutex); // 锁定错误输出
                std::cerr << "SerialPort::receive: read failed. Error: " << strerror(errno) << std::endl;
            }
            return -1; // 指示错误
        }

        return static_cast<int>(received); // 返回读取到的字节数 (>= 0)
#endif
    }

    void SerialPort::close()
    {
        LIBLSX::LockManager::LockGuard<std::mutex> lock(mtx); // 锁定互斥锁

#ifdef _WIN32
        if (hComm != INVALID_HANDLE_VALUE)
        {
            CloseHandle(hComm);
            hComm = INVALID_HANDLE_VALUE; // 设置句柄为无效状态
            // {
            //     std::lock_guard<std::mutex> lock_out(g_error_mutex); // 锁定输出
            //     std::cout << "SerialPort: Windows port closed." << std::endl;
            // }
        }
#else // POSIX
        if (fd >= 0)
        {
            // 恢复原始设置 (可选，但好的实践)
            tcsetattr(fd, TCSANOW, &oldtio);
            ::close(fd);
            fd = -1; // 设置句柄为无效状态
            // {
            //     std::lock_guard<std::mutex> lock_out(g_error_mutex); // 锁定输出
            //     std::cout << "SerialPort: POSIX port closed." << std::endl;
            // }
        }
#endif
    }

    SerialPort::~SerialPort()
    {
        close(); // 确保在析构时调用 close
    }

    bool SerialPort::setSendTimeout(int timeout_ms)
    {
        LIBLSX::LockManager::LockGuard<std::mutex> lock(mtx); // 锁定互斥锁
        // 串口的超时通常通过 VTIME/VMIN (POSIX) 或 COMMTIMEOUTS (Windows) 配置，
        // 直接映射到 ICommunication 的通用毫秒超时比较复杂，且行为可能不完全一致。
        // 例如，POSIX 的 VTIME 单位是 0.1 秒，且范围有限 (0-255)。
        // Windows 的 COMMTIMEOUTS 更灵活，但需要精确配置 ReadTotalTimeoutConstant/Multiplier 等。
        // 此处仅提供接口，具体实现需要根据所需的串口超时行为来完成。
        LIBLSX::LockManager::LockGuard<std::mutex> lock(g_error_mutex); // 锁定错误输出
        std::cerr <<
            "SerialPort::setSendTimeout: Generic timeout setting may not be fully supported or configured via VTIME/VMIN/COMMTIMEOUTS."
            << std::endl;
        // 如果需要实现，您可以在这里根据 timeout_ms 配置 VTIME/VMIN 或 COMMTIMEOUTS
        // 例如，对于 POSIX read timeout:
        // if (fd >= 0) {
        //     struct termios current_tio;
        //     tcgetattr(fd, &current_tio);
        //     if (timeout_ms >= 0) {
        //         current_tio.c_cc[VMIN] = 0; // Return after VTIME elapsed
        //         current_tio.c_cc[VTIME] = timeout_ms / 100; // Timeout in 0.1s intervals (max 25.5s)
        //         if (tcsetattr(fd, TCSANOW, &current_tio) < 0) return false;
        //     } else { // Blocking read (timeout_ms = -1)
        //          current_tio.c_cc[VMIN] = 1; // Wait for at least 1 byte
        //          current_tio.c_cc[VTIME] = 0; // No inter-character timer
        //          if (tcsetattr(fd, TCSANOW, &current_tio) < 0) return false;
        //     }
        //     return true;
        // }
        // 对于 Windows Write Timeout，需要设置 COMMTIMEOUTS 的 WriteTotalTimeoutConstant/Multiplier
        // ... Windows implementation ...

        return false; // 指示未完全实现/支持通用超时
    }

    bool SerialPort::setReceiveTimeout(int timeout_ms)
    {
        LIBLSX::LockManager::LockGuard<std::mutex> lock(mtx); // 锁定互斥锁
        // 串口的超时配置同上
        LIBLSX::LockManager::LockGuard<std::mutex> lock(g_error_mutex); // 锁定错误输出
        std::cerr <<
            "SerialPort::setReceiveTimeout: Generic timeout setting may not be fully supported or configured via VTIME/VMIN/COMMTIMEOUTS."
            << std::endl;
        // 如果需要实现，您可以在这里根据 timeout_ms 配置 VTIME/VMIN 或 COMMTIMEOUTS
        // 例如，对于 POSIX read timeout (同 setSendTimeout 中的 POSIX read timeout 配置):
        // if (fd >= 0) {
        //     struct termios current_tio;
        //     tcgetattr(fd, &current_tio);
        //     if (timeout_ms >= 0) {
        //         current_tio.c_cc[VMIN] = 0;
        //         current_tio.c_cc[VTIME] = timeout_ms / 100;
        //         if (tcsetattr(fd, TCSANOW, &current_tio) < 0) return false;
        //     } else { // Blocking read
        //          current_tio.c_cc[VMIN] = 1;
        //          current_tio.c_cc[VTIME] = 0;
        //          if (tcsetattr(fd, TCSANOW, &current_tio) < 0) return false;
        //     }
        //     return true;
        // }
        // 对于 Windows Read Timeout，需要设置 COMMTIMEOUTS 的 ReadIntervalTimeout/ReadTotalTimeoutConstant/Multiplier
        // ... Windows implementation ...

        return false; // 指示未完全实现/支持通用超时
    }
} // namespace LSX_LIB
