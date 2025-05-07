// LSXTransportLib: 数据传输工具库（跨平台）
// 命名空间：LSX_LIB

// ---------- File: WinsockManager.h ----------
#ifndef LSX_WINSOCK_MANAGER_H
#define LSX_WINSOCK_MANAGER_H
#pragma once
#ifdef _WIN32
#include <winsock2.h>
#pragma comment(lib,"ws2_32.lib") // 链接 ws2_32.lib 库
#endif

#include <iostream> // 用于错误输出
#include <mutex> // 用于保护错误输出

#include "GlobalErrorMutex.h"

namespace LSX_LIB::DataTransfer {

#ifdef _WIN32
    class WinsockManager {
    public:
        WinsockManager() {
            WSADATA wsa;
            int ret = WSAStartup(MAKEWORD(2, 2), &wsa);
            if (ret != 0) {
                // 使用锁保护全局错误输出
                std::lock_guard<std::mutex> lock(g_error_mutex);
                std::cerr << "WinsockManager: WSAStartup failed. Error Code: " << WSAGetLastError() << std::endl;
                // 根据需求，可能抛出异常或设置标志
            } else {
                // 使用锁保护全局错误输出
                // std::lock_guard<std::mutex> lock(g_error_mutex);
                // std::cout << "WinsockManager: WSAStartup successful." << std::endl;
            }
        }

        ~WinsockManager() {
            // 在清理前检查是否已初始化（更健壮的实现会跟踪初始化状态）
            WSACleanup();
            // 使用锁保护全局错误输出
            // std::lock_guard<std::mutex> lock(g_error_mutex);
            // std::cout << "WinsockManager: WSACleanup successful." << std::endl;
        }

        // 禁止拷贝和赋值
        WinsockManager(const WinsockManager&) = delete;
        WinsockManager& operator=(const WinsockManager&) = delete;
    };

    // 静态实例，确保随程序启动/结束自动初始化/清理
    // 库的使用者需要在主编译单元中确保这个静态对象存在
    // 或者如果不是静态方式，需要显式调用初始化/清理函数
#endif // _WIN32

} // namespace LSX_LIB

#endif // LSX_WINSOCK_MANAGER_H