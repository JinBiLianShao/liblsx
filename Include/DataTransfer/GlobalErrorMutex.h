/**
* @file GlobalErrorMutex.h
 * @brief 数据传输工具库 - 全局错误互斥锁
 * @details 定义了一个全局的互斥锁 `g_error_mutex`，用于在多线程环境中同步对共享错误输出流（如 `std::cerr`）的访问。
 * 这有助于防止多个线程同时向错误输出写入，导致输出混乱或数据竞争。
 * @author 连思鑫（liansixin）
 * @date 2025-4-8
 * @version 1.0
 *
 * ### 核心功能
 * - **线程安全**: 提供一个全局可用的互斥锁，用于保护对共享资源的访问。
 * - **错误输出同步**: 主要设计用于在多线程日志记录或错误报告时，同步对标准错误流的访问。
 *
 * ### 使用示例
 *
 * @code
 * #include "GlobalErrorMutex.h"
 * #include <iostream>
 * #include <thread>
 * #include <vector>
 *
 * void worker_function(int id) {
 * // 在访问全局错误输出前锁定互斥锁
 * std::lock_guard<std::mutex> lock(LSX_LIB::DataTransfer::g_error_mutex);
 *
 * std::cerr << "线程 " << id << " 正在写入错误信息。" << std::endl;
 * // 互斥锁在 lock_guard 对象超出作用域时自动解锁
 * }
 *
 * int main() {
 * std::vector<std::thread> threads;
 * for (int i = 0; i < 5; ++i) {
 * threads.emplace_back(worker_function, i);
 * }
 *
 * for (auto& t : threads) {
 * t.join();
 * }
 *
 * std::cout << "所有线程完成。" << std::endl;
 * return 0;
 * }
 * @endcode
 *
 * ### 注意事项
 * - **使用范围**: 此互斥锁是全局的，应谨慎使用，避免过度锁定影响性能。
 * - **锁定粒度**: 仅在需要同步访问共享错误输出时锁定，避免在持有锁时执行耗时操作。
 * - **RAII**: 推荐使用 `std::lock_guard` 或 `std::unique_lock` 来管理互斥锁的生命周期，确保在异常发生时也能正确解锁。
 */

// LSXTransportLib: 数据传输工具库（跨平台）
// 命名空间：LSX_LIB

#ifndef LSX_GLOBAL_ERROR_MUTEX_H
#define LSX_GLOBAL_ERROR_MUTEX_H
#pragma once
#include <mutex> // 包含 std::mutex

/**
 * @brief LSX 库的根命名空间。
 */
namespace LSX_LIB
{
    /**
     * @brief 数据传输相关的命名空间。
     * 包含各种通信类和工具。
     */
    namespace DataTransfer {

        /**
         * @brief 用于保护全局错误输出的互斥锁。
         * 在多线程环境中，当多个线程可能同时向标准错误流 (std::cerr) 写入时，
         * 应使用此互斥锁来同步访问，防止输出交错或数据竞争。
         */
        extern std::mutex g_error_mutex;

    } // namespace DataTransfer
} // namespace LSX_LIB

#endif // LSX_GLOBAL_ERROR_MUTEX_H
