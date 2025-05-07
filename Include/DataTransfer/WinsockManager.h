/**
 * @file WinsockManager.h
 * @brief 数据传输工具库 - Winsock 初始化管理器 (Windows)
 * @details 定义了 LSX_LIB::DataTransfer 命名空间下的 WinsockManager 类，
 * 这是一个辅助类，用于在 Windows 平台上管理 Winsock API 的初始化和清理。
 * 使用 RAII (Resource Acquisition Is Initialization) 模式，通过对象的构造函数调用 `WSAStartup`
 * 进行初始化，在析构函数中调用 `WSACleanup` 进行清理。
 * 这确保了在程序生命周期内 Winsock 被正确初始化和释放，避免资源泄露或未定义行为。
 * 此类仅在 Windows 平台下有效，通过 `#ifdef _WIN32` 进行条件编译。
 * @author 连思鑫（liansixin）
 * @date 2025-4-8
 * @version 1.0
 *
 * ### 核心功能
 * - **Winsock 初始化**: 在对象构造时自动调用 `WSAStartup`。
 * - **Winsock 清理**: 在对象析构时自动调用 `WSACleanup`。
 * - **RAII 模式**: 简化 Winsock 的生命周期管理。
 * - **错误报告**: 在初始化失败时使用全局错误互斥锁同步输出错误信息。
 * - **线程安全**: 使用互斥锁保护错误输出。
 *
 * ### 使用示例
 *
 * @code
 * #include "WinsockManager.h"
 * #include "TcpClient.h" // 假设需要使用其他依赖 Winsock 的类
 * #include <iostream>
 *
 * // 在 main 函数或程序入口处创建一个 WinsockManager 的实例
 * // 确保其生命周期覆盖所有需要使用 Winsock 的代码
 * // 如果是库，可能需要在库的初始化函数中创建并持有这个对象
 *
 * int main() {
 * #ifdef _WIN32
 * // 创建 Winsock 管理器实例。
 * // 它的构造函数会调用 WSAStartup，析构函数会在 main 结束时调用 WSACleanup。
 * LSX_LIB::DataTransfer::WinsockManager winsock_manager;
 *
 * // 现在可以使用依赖 Winsock 的类，例如 TcpClient
 * try {
 * LSX_LIB::DataTransfer::TcpClient client("127.0.0.1", 12345);
 * if (client.create()) {
 * std::cout << "TCP 客户端创建成功 (依赖 Winsock)。" << std::endl;
 * // ... 进行 TCP 通信 ...
 * client.close();
 * } else {
 * std::cerr << "TCP 客户端创建失败。" << std::endl;
 * }
 * } catch (const std::exception& e) {
 * std::cerr << "发生异常: " << e.what() << std::endl;
 * }
 *
 * // winsock_manager 对象在 main 函数结束时超出作用域，其析构函数会自动调用 WSACleanup
 * #else
 * std::cout << "WinsockManager 仅在 Windows 平台下有效。" << std::endl;
 * #endif
 *
 * std::cout << "程序结束。" << std::endl;
 * return 0;
 * }
 * @endcode
 *
 * ### 注意事项
 * - **平台依赖**: 此类仅适用于 Windows 平台。在非 Windows 平台下，整个类会被条件编译排除。
 * - **单例模式**: 通常在一个应用程序中只需要初始化 Winsock 一次。可以通过确保只创建一个 WinsockManager 实例（例如，作为全局静态变量或单例）来实现。
 * - **初始化时机**: 确保 WinsockManager 对象在任何需要使用 Winsock 功能的代码之前被创建。
 * - **清理时机**: 确保 WinsockManager 对象在所有使用 Winsock 功能的代码之后才被销毁。
 * - **错误处理**: `WSAStartup` 失败会导致错误信息输出到 `std::cerr`。更复杂的应用可能需要更精细的错误处理机制（例如，抛出异常或设置全局错误状态）。
 */

// LSXTransportLib: 数据传输工具库（跨平台）
// 命名空间：LSX_LIB

#ifndef LSX_WINSOCK_MANAGER_H
#define LSX_WINSOCK_MANAGER_H
#pragma once

// 仅在 Windows 平台下包含 Winsock 头文件和定义 WinsockManager
#ifdef _WIN32
#include <winsock2.h> // 包含 Winsock 主要头文件，用于 WSAStartup, WSACleanup, WSAGetLastError 等
#pragma comment(lib,"ws2_32.lib") // 链接 ws2_32.lib 库，包含 Winsock 函数的实现
#endif

#include <iostream> // 用于错误输出 (std::cerr)
#include <mutex> // 用于保护错误输出 (std::mutex)

#include "GlobalErrorMutex.h" // 包含全局错误锁头文件 (用于同步错误输出)

/**
 * @brief LSX 库的根命名空间。
 */
namespace LSX_LIB
{
    /**
     * @brief 数据传输相关的命名空间。
     * 包含各种通信类和工具。
     */
    namespace DataTransfer
    {
#ifdef _WIN32
        /**
         * @brief Winsock 初始化和清理管理器类 (仅限 Windows)。
         * 使用 RAII 模式确保在对象生命周期内正确初始化和清理 Winsock API。
         * 构造函数调用 WSAStartup，析构函数调用 WSACleanup。
         */
        class WinsockManager {
        public:
            /**
             * @brief 构造函数。
             * 调用 `WSAStartup` 初始化 Winsock API。
             * 如果初始化失败，会使用全局错误互斥锁同步输出错误信息到 `std::cerr`。
             */
            WinsockManager() {
                WSADATA wsa;
                // 请求 Winsock 2.2 版本
                int ret = WSAStartup(MAKEWORD(2, 2), &wsa);
                if (ret != 0) {
                    // 使用锁保护全局错误输出
                    std::lock_guard<std::mutex> lock(g_error_mutex);
                    std::cerr << "WinsockManager: WSAStartup failed. Error Code: " << WSAGetLastError() << std::endl;
                    // 根据需求，可能抛出异常或设置标志以指示初始化失败
                } else {
                    // 初始化成功，可选地输出成功信息
                    // 使用锁保护全局错误输出
                    // std::lock_guard<std::mutex> lock(g_error_mutex);
                    // std::cout << "WinsockManager: WSAStartup successful." << std::endl;
                }
            }

            /**
             * @brief 析构函数。
             * 调用 `WSACleanup` 清理 Winsock API 资源。
             * 在程序退出或对象生命周期结束时自动调用。
             */
            ~WinsockManager() {
                // 在清理前检查是否已初始化（更健壮的实现会跟踪初始化状态）
                // 当前实现依赖于 WSAStartup/WSACleanup 的幂等性或检查内部状态
                WSACleanup();
                // 清理成功，可选地输出清理信息
                // 使用锁保护全局错误输出
                // std::lock_guard<std::mutex> lock(g_error_mutex);
                // std::cout << "WinsockManager: WSACleanup successful." << std::endl;
            }

            // 禁止拷贝和赋值，确保只有一个 WinsockManager 实例负责初始化和清理
            /**
             * @brief 禁用拷贝构造函数。
             * 确保 WinsockManager 对象不可拷贝。
             */
            WinsockManager(const WinsockManager&) = delete;
            /**
             * @brief 禁用赋值运算符。
             * 确保 WinsockManager 对象不可赋值。
             */
            WinsockManager& operator=(const WinsockManager&) = delete;
        };

        // 静态实例，确保随程序启动/结束自动初始化/清理
        // 库的使用者需要在主编译单元中确保这个静态对象存在
        // 或者如果不是静态方式，需要显式调用初始化/清理函数
        // 例如，在某个 .cpp 文件中定义:
        // LSX_LIB::DataTransfer::WinsockManager g_winsock_manager_instance;
#endif // _WIN32
    } // namespace DataTransfer
} // namespace LSX_LIB

#endif // LSX_WINSOCK_MANAGER_H
