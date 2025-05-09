/**
 * @file SharedLockGuard.h
 * @brief 共享锁守卫类 (模板)
 * @details 定义了 LIBLSX::LockManager 命名空间下的 SharedLockGuard 类，
 * 这是一个模板类，用于实现 RAII (Resource Acquisition Is Initialization) 风格的共享锁管理。
 * 它内部使用 std::shared_lock 封装了任意满足 SharedMutex 要求（即可共享锁定的互斥量）的类型，
 * 确保在 SharedLockGuard 对象生命周期内持有共享锁，并在对象超出作用域时自动释放。
 * 提供了构造时共享锁定、延迟锁定、尝试共享锁定和带超时的尝试共享锁定（针对定时共享互斥量）等功能。
 * 类禁用了拷贝和赋值，以确保锁的所有权不会被意外转移或复制。
 * @author 连思鑫（liansixin）
 * @date 2025年5月9日
 * @version 1.0
 *
 * ### 核心功能
 * - **RAII 共享锁管理**: 自动在构造时获取共享锁，在析构时释放共享锁。
 * - **通用共享互斥量支持**: 可与 std::shared_mutex, std::shared_timed_mutex 等满足 SharedMutex 要求的互斥量类型配合使用。
 * - **延迟锁定**: 支持构造时不立即共享锁定互斥量。
 * - **尝试共享锁定**: 提供非阻塞的 `try_lock_shared` 方法。
 * - **带超时尝试共享锁定**: 提供 `try_lock_shared_for` 方法，支持对定时共享互斥量进行带超时的共享锁定尝试。
 * - **锁所有权检查**: `owns_lock` 方法检查当前 SharedLockGuard 对象是否持有锁。
 * - **线程安全**: 类本身通过正确使用 std::shared_lock 来管理底层互斥量，确保线程安全。
 *
 * ### 使用示例
 *
 * @code
 * #include "SharedLockGuard.h" // 包含 SharedLockGuard 类定义
 * #include <iostream>
 * #include <thread>
 * #include <shared_mutex> // For std::shared_mutex, std::shared_timed_mutex
 * #include <vector>
 * #include <chrono>
 *
 * namespace LIBLSX {
 * namespace LockManager {
 * // Forward declaration for example usage
 * template <typename SharedMutexType> class SharedLockGuard;
 * }
 * }
 *
 * std::shared_mutex g_shared_mutex; // 一个标准的共享互斥量
 * std::shared_timed_mutex g_shared_timed_mutex; // 一个定时共享互斥量
 *
 * int g_shared_data = 0; // 共享资源
 *
 * // 示例：多个线程同时持有共享锁进行读操作
 * void read_shared_resource(int id) {
 * std::cout << "[Reader Thread " << id << "] Attempting to acquire shared lock..." << std::endl;
 * // 使用 SharedLockGuard 自动锁定 g_shared_mutex (共享模式)
 * LIBLSX::LockManager::SharedLockGuard<std::shared_mutex> lock(g_shared_mutex);
 * std::cout << "[Reader Thread " << id << "] Shared lock acquired." << std::endl;
 *
 * // 访问共享资源 (读操作)
 * std::cout << "[Reader Thread " << id << "] Reading shared resource: " << g_shared_data << std::endl;
 *
 * std::this_thread::sleep_for(std::chrono::milliseconds(50)); // 模拟读操作时间
 *
 * std::cout << "[Reader Thread " << id << "] Releasing shared lock." << std::endl;
 * } // SharedLockGuard 对象超出作用域，析构时自动释放共享锁
 *
 * // 示例：一个线程持有独占锁进行写操作
 * void write_shared_resource(int id) {
 * std::cout << "[Writer Thread " << id << "] Attempting to acquire unique lock..." << std::endl;
 * // 使用 std::unique_lock 获取独占锁 (std::shared_mutex 也支持 unique_lock)
 * std::unique_lock<std::shared_mutex> lock(g_shared_mutex);
 * std::cout << "[Writer Thread " << id << "] Unique lock acquired." << std::endl;
 *
 * // 修改共享资源 (写操作)
 * g_shared_data++;
 * std::cout << "[Writer Thread " << id << "] Shared resource incremented to: " << g_shared_data << std::endl;
 *
 * std::this_thread::sleep_for(std::chrono::milliseconds(100)); // 模拟写操作时间
 *
 * std::cout << "[Writer Thread " << id << "] Releasing unique lock." << std::endl;
 * } // unique_lock 对象超出作用域，析构时自动释放独占锁
 *
 * // 示例：使用 SharedLockGuard 进行带超时的尝试共享锁定
 * void timed_read_resource(int id) {
 * std::cout << "[Timed Reader Thread " << id << "] Attempting to acquire shared timed lock with timeout..." << std::endl;
 *
 * // 使用 SharedLockGuard 进行带超时的尝试共享锁定
 * LIBLSX::LockManager::SharedLockGuard<std::shared_timed_mutex> lock(g_shared_timed_mutex, std::defer_lock); // 延迟锁定
 *
 * // 尝试共享锁定，最多等待 50 毫秒
 * if (lock.try_lock_shared_for(std::chrono::milliseconds(50))) {
 * std::cout << "[Timed Reader Thread " << id << "] Shared timed lock acquired." << std::endl;
 *
 * // 访问共享资源
 * // Note: Accessing g_shared_data here would require g_shared_timed_mutex
 * // to protect it, or careful synchronization with g_shared_mutex.
 * // For simplicity, we'll just indicate success.
 *
 * std::cout << "[Timed Reader Thread " << id << "] Performing timed read operation." << std::endl;
 * std::this_thread::sleep_for(std::chrono::milliseconds(30)); // 模拟工作
 *
 * std::cout << "[Timed Reader Thread " << id << "] Releasing shared timed lock." << std::endl;
 * } else {
 * std::cout << "[Timed Reader Thread " << id << "] Failed to acquire shared timed lock within timeout." << std::endl;
 * } // SharedLockGuard 对象超出作用域，如果持有锁则释放
 * }
 *
 * int main() {
 * // 启动多个读线程和一个写线程
 * std::vector<std::thread> reader_threads;
 * for (int i = 0; i < 5; ++i) {
 * reader_threads.emplace_back(read_shared_resource, i);
 * }
 * std::thread writer_t(write_shared_resource, 1);
 *
 * for (auto& t : reader_threads) {
 * t.join();
 * }
 * writer_t.join();
 *
 * std::cout << "All standard threads finished. Final shared data: " << g_shared_data << std::endl;
 *
 * std::vector<std::thread> timed_reader_threads;
 * for (int i = 0; i < 5; ++i) {
 * timed_reader_threads.emplace_back(timed_read_resource, i);
 * }
 *
 * for (auto& t : timed_reader_threads) {
 * t.join();
 * }
 *
 * std::cout << "All timed reader threads finished." << std::endl;
 *
 * std::cout << "主线程退出。" << std::endl;
 * return 0;
 * }
 * @endcode
 *
 * ### 注意事项
 * - **锁类型要求**: SharedLockGuard 要求 SharedMutexType 满足 SharedMutex 要求（即支持共享锁定）。`try_lock_shared_for` 方法额外要求 SharedMutexType 是定时共享互斥量类型（如 `std::shared_timed_mutex`）。
 * - **所有权**: SharedLockGuard 在构造时（除非使用 `std::defer_lock`）获取共享锁的所有权，并在析构时释放。确保 SharedLockGuard 对象的生命周期与需要保护的共享临界区范围一致。
 * - **不可拷贝/赋值**: SharedLockGuard 对象不可拷贝或赋值，以防止多个 SharedLockGuard 对象管理同一个互斥量，导致未定义行为（如双重解锁）。
 * - **`try_lock_shared_for` 行为**: `try_lock_shared_for` 方法仅对定时共享互斥量有效。对于非定时共享互斥量，它会回退到调用 `try_lock_shared`。
 * - **底层 Shared_Lock**: SharedLockGuard 内部使用了 `std::shared_lock`，因此它继承了 `std::shared_lock` 的一些特性。
 * - **读写分离**: 共享互斥量允许多个线程同时持有共享锁（用于读），但只允许一个线程持有独占锁（用于写）。在设计时需要明确哪些操作需要共享锁，哪些需要独占锁。
 */

#ifndef LIBLSX_LOCK_MANAGER_SHARED_LOCK_GUARD_H
#define LIBLSX_LOCK_MANAGER_SHARED_LOCK_GUARD_H
#pragma once

#include <shared_mutex> // For std::shared_lock, std::shared_mutex, std::shared_timed_mutex
#include <chrono> // For std::chrono::duration
#include <type_traits> // For std::is_same_v
#include <utility> // For std::forward (if needed in template methods)


/**
 * @brief LSX 库的根命名空间。
 */
namespace LIBLSX {
    /**
     * @brief 锁管理相关的命名空间。
     * 包含锁和同步原语的封装。
     */
    namespace LockManager {

        /**
         * @brief 共享锁守卫类 (模板)。
         * 实现 RAII 风格的共享互斥量锁管理。
         * 封装 std::shared_lock，确保在对象生命周期内持有共享锁并在超出作用域时自动释放。
         * 可与任何满足 SharedMutex 要求（即可共享锁定）的互斥量类型配合使用。
         * @tparam SharedMutexType 满足 SharedMutex 要求的互斥量类型。
         */
        template <typename SharedMutexType>
        class SharedLockGuard {
        public:
            /**
             * @brief 构造函数。
             * 构造一个 SharedLockGuard 对象，并尝试立即获取指定互斥量 `m` 的共享锁。
             * 如果获取失败，会阻塞直到获取共享锁。
             *
             * @param m 要共享锁定的互斥量。
             */
            explicit SharedLockGuard(SharedMutexType& m)
                : lock_(m) {}

            /**
             * @brief 构造函数（延迟锁定）。
             * 构造一个 SharedLockGuard 对象，但不立即获取指定互斥量 `m` 的共享锁。
             * 互斥量 `m` 必须在构造时处于未锁定状态。
             *
             * @param m 要管理的互斥量。
             * @param tag `std::defer_lock` 标签，指示延迟锁定。
             */
            SharedLockGuard(SharedMutexType& m, std::defer_lock_t) noexcept
                : lock_(m, std::defer_lock) {}

            /**
             * @brief 禁用拷贝构造函数。
             * SharedLockGuard 对象管理锁的所有权，不可拷贝。
             */
            SharedLockGuard(const SharedLockGuard&) = delete;
            /**
             * @brief 禁用赋值运算符。
             * SharedLockGuard 对象管理锁的所有权，不可赋值。
             */
            SharedLockGuard& operator=(const SharedLockGuard&) = delete;

            /**
             * @brief 析构函数。
             * 在 SharedLockGuard 对象超出作用域时自动调用。
             * 如果 SharedLockGuard 对象当前持有共享锁，析构函数会自动释放该共享锁。
             */
            ~SharedLockGuard() noexcept = default;

            /**
             * @brief 尝试获取互斥量的共享锁。
             * 尝试非阻塞地获取关联互斥量的共享锁。
             *
             * @return 如果成功获取共享锁，返回 true；否则返回 false。
             */
            bool try_lock_shared() {
                return lock_.try_lock(); // std::shared_lock::try_lock() attempts shared lock
            }

            /**
             * @brief 尝试获取互斥量的共享锁，最多等待指定的时间。
             * 尝试获取关联互斥量的共享锁，最多等待 `timeout` 指定的时间。
             * 此方法仅对定时共享互斥量类型 (std::shared_timed_mutex) 有效。
             * 对于其他共享互斥量类型，它会回退到调用 `try_lock_shared`。
             *
             * @tparam Rep 表示时间间隔的算术类型。
             * @tparam Period 表示时间单位的 std::ratio 类型。
             * @param timeout 最大等待时间。
             * @return 如果成功获取共享锁，返回 true；如果超时或未能获取共享锁，返回 false。
             */
            template <typename Rep, typename Period>
            bool try_lock_shared_for(const std::chrono::duration<Rep, Period>& timeout) {
                // 使用 if constexpr 在编译时根据 SharedMutexType 选择不同的行为
                if constexpr (std::is_same_v<SharedMutexType, std::shared_timed_mutex>) {
                    // 对于定时共享互斥量，调用其 try_lock_shared_for 方法
                    return lock_.try_lock_for(timeout); // std::shared_lock::try_lock_for() attempts shared lock
                } else {
                    // 对于非定时共享互斥量，只调用 try_lock_shared
                    return lock_.try_lock(); // std::shared_lock::try_lock() attempts shared lock
                }
            }

            /**
             * @brief 检查 SharedLockGuard 对象是否持有锁。
             *
             * @return 如果 SharedLockGuard 对象当前持有其关联互斥量的锁（共享或独占，取决于 std::shared_lock 的状态），返回 true；否则返回 false。
             */
            bool owns_lock() const noexcept {
                return lock_.owns_lock();
            }

            // Note: SharedLockGuard does not expose lock() or unlock() directly
            // as its purpose is RAII for shared locking. Use std::shared_lock directly for manual control.

        private:
            /**
             * @brief 底层 std::shared_lock 对象。
             * 管理关联共享互斥量的共享锁定状态和所有权。
             */
            std::shared_lock<SharedMutexType> lock_;
        };

    } // namespace LockManager
} // namespace LIBLSX

#endif // LIBLSX_LOCK_MANAGER_SHARED_LOCK_GUARD_H
