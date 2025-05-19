/**
 * @file LockGuard.h
 * @brief 锁守卫类 (模板)
 * @details 定义了 LIBLSX::LockManager 命名空间下的 LockGuard 类，
 * 这是一个模板类，用于实现 RAII (Resource Acquisition Is Initialization) 风格的锁管理。
 * 它内部使用 std::unique_lock 封装了任意满足 BasicLockable 要求的互斥量类型，
 * 确保在 LockGuard 对象生命周期内持有锁，并在对象超出作用域时自动释放锁。
 * 提供了构造时锁定、延迟锁定、尝试锁定和带超时的尝试锁定（针对定时互斥量）等功能。
 * 类禁用了拷贝和赋值，以确保锁的所有权不会被意外转移或复制。
 * @author 连思鑫（liansixin）
 * @date 2025年5月9日
 * @version 1.0
 *
 * ### 核心功能
 * - **RAII 锁管理**: 自动在构造时获取锁，在析构时释放锁。
 * - **通用互斥量支持**: 可与 std::mutex, std::recursive_mutex, std::timed_mutex, std::recursive_timed_mutex 等满足 BasicLockable 要求的互斥量类型配合使用。
 * - **延迟锁定**: 支持构造时不立即锁定互斥量。
 * - **尝试锁定**: 提供非阻塞的 `try_lock` 方法。
 * - **带超时尝试锁定**: 提供 `try_lock_for` 方法，支持对定时互斥量进行带超时的锁定尝试。
 * - **锁所有权检查**: `owns_lock` 方法检查当前 LockGuard 对象是否持有锁。
 * - **线程安全**: 类本身通过正确使用 std::unique_lock 来管理底层互斥量，确保线程安全。
 *
 * ### 使用示例
 *
 * @code
 * #include "LockGuard.h" // 包含 LockGuard 类定义
 * #include <iostream>
 * #include <thread>
 * #include <mutex>
 * #include <vector>
 * #include <chrono>
 *
 * namespace LIBLSX {
 * namespace LockManager {
 * // Forward declaration for example usage
 * template <typename MutexType> class LockGuard;
 * }
 * }
 *
 * std::mutex g_mutex; // 一个标准的互斥量
 * std::timed_mutex g_timed_mutex; // 一个定时互斥量
 * int g_shared_resource = 0;
 *
 * // 示例：使用 LockGuard 保护共享资源
 * void increment_shared_resource(int id) {
 * std::cout << "[Thread " << id << "] Attempting to acquire mutex..." << std::endl;
 * // 使用 LockGuard 自动锁定 g_mutex
 * LIBLSX::LockManager::LockGuard<std::mutex> lock(g_mutex);
 * std::cout << "[Thread " << id << "] Mutex acquired." << std::endl;
 *
 * // 访问共享资源
 * g_shared_resource++;
 * std::cout << "[Thread " << id << "] Shared resource incremented to: " << g_shared_resource << std::endl;
 *
 * std::this_thread::sleep_for(std::chrono::milliseconds(100)); // 模拟工作
 *
 * std::cout << "[Thread " << id << "] Releasing mutex." << std::endl;
 * } // LockGuard 对象超出作用域，析构时自动释放锁
 *
 * // 示例：使用 LockGuard 进行带超时的尝试锁定
 * void timed_increment_resource(int id) {
 * std::cout << "[Timed Thread " << id << "] Attempting to acquire timed mutex with timeout..." << std::endl;
 *
 * // 使用 LockGuard 进行带超时的尝试锁定
 * LIBLSX::LockManager::LockGuard<std::timed_mutex> lock(g_timed_mutex, std::defer_lock); // 延迟锁定
 *
 * // 尝试锁定，最多等待 50 毫秒
 * if (lock.try_lock_for(std::chrono::milliseconds(50))) {
 * std::cout << "[Timed Thread " << id << "] Timed mutex acquired." << std::endl;
 *
 * // 访问共享资源
 * // Note: g_shared_resource is protected by g_mutex in the other example,
 * // but here we simulate a separate resource protected by g_timed_mutex.
 * // In a real scenario, ensure consistent mutex usage for resources.
 * // For demonstration, we'll use a local variable.
 * // g_shared_resource++; // Would need careful consideration if same resource
 *
 * std::cout << "[Timed Thread " << id << "] Performing timed operation." << std::endl;
 * std::this_thread::sleep_for(std::chrono::milliseconds(30)); // 模拟工作
 *
 * std::cout << "[Timed Thread " << id << "] Releasing timed mutex." << std::endl;
 * } else {
 * std::cout << "[Timed Thread " << id << "] Failed to acquire timed mutex within timeout." << std::endl;
 * } // LockGuard 对象超出作用域，如果持有锁则释放
 * }
 *
 * int main() {
 * std::vector<std::thread> threads;
 * for (int i = 0; i < 5; ++i) {
 * threads.emplace_back(increment_shared_resource, i);
 * }
 *
 * for (auto& t : threads) {
 * t.join();
 * }
 *
 * std::cout << "All standard threads finished. Final shared resource: " << g_shared_resource << std::endl;
 *
 * std::vector<std::thread> timed_threads;
 * for (int i = 0; i < 5; ++i) {
 * timed_threads.emplace_back(timed_increment_resource, i);
 * }
 *
 * for (auto& t : timed_threads) {
 * t.join();
 * }
 *
 * std::cout << "All timed threads finished." << std::endl;
 *
 * std::cout << "主线程退出。" << std::endl;
 * return 0;
 * }
 * @endcode
 *
 * ### 注意事项
 * - **锁类型要求**: LockGuard 要求 MutexType 满足 BasicLockable 要求。`try_lock_for` 方法额外要求 MutexType 是定时互斥量类型（如 `std::timed_mutex`, `std::recursive_timed_mutex`）。
 * - **所有权**: LockGuard 在构造时（除非使用 `std::defer_lock`）获取锁的所有权，并在析构时释放。确保 LockGuard 对象的生命周期与需要保护的临界区范围一致。
 * - **不可拷贝/赋值**: LockGuard 对象不可拷贝或赋值，以防止多个 LockGuard 对象管理同一个互斥量，导致未定义行为（如双重解锁）。
 * - **`try_lock_for` 行为**: `try_lock_for` 方法仅对定时互斥量有效。对于非定时互斥量，它会回退到调用 `try_lock`。
 * - **底层 Unique_Lock**: LockGuard 内部使用了 `std::unique_lock`，因此它继承了 `std::unique_lock` 的一些特性，例如可以与条件变量一起使用（尽管此 LockGuard 类本身没有直接暴露条件变量的 `wait` 接口）。
 */

#ifndef LIBLSX_LOCK_MANAGER_LOCK_GUARD_H
#define LIBLSX_LOCK_MANAGER_LOCK_GUARD_H
#pragma once

#include <mutex> // For std::unique_lock, std::mutex, std::timed_mutex etc.
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
         * @brief 锁守卫类 (模板)。
         * 实现 RAII 风格的互斥量锁管理。
         * 封装 std::unique_lock，确保在对象生命周期内持有锁并在超出作用域时自动释放。
         * 可与任何满足 BasicLockable 要求的互斥量类型配合使用。
         * @tparam MutexType 满足 BasicLockable 要求的互斥量类型。
         */
        template<typename MutexType>
        class LockGuard {
        public:
            /**
             * @brief 构造函数。
             * 构造一个 LockGuard 对象，并尝试立即锁定指定的互斥量 `m`。
             * 如果锁定失败，会阻塞直到获取锁。
             *
             * @param m 要锁定的互斥量。
             */
            explicit LockGuard(MutexType &m)
                    : lock_(m) {}

            /**
             * @brief 构造函数（延迟锁定）。
             * 构造一个 LockGuard 对象，但不立即锁定指定的互斥量 `m`。
             * 互斥量 `m` 必须在构造时处于未锁定状态。
             *
             * @param m 要管理的互斥量。
             * @param tag `std::defer_lock` 标签，指示延迟锁定。
             */
            LockGuard(MutexType &m, std::defer_lock_t) noexcept
                    : lock_(m, std::defer_lock) {}

            /**
             * @brief 禁用拷贝构造函数。
             * LockGuard 对象管理锁的所有权，不可拷贝。
             */
            LockGuard(const LockGuard &) = delete;

            /**
             * @brief 禁用赋值运算符。
             * LockGuard 对象管理锁的所有权，不可赋值。
             */
            LockGuard &operator=(const LockGuard &) = delete;

            /**
             * @brief 析构函数。
             * 在 LockGuard 对象超出作用域时自动调用。
             * 如果 LockGuard 对象当前持有锁，析构函数会自动释放该锁。
             */
            ~LockGuard() noexcept = default;

            /**
             * @brief 尝试锁定互斥量。
             * 尝试非阻塞地锁定关联的互斥量。
             *
             * @return 如果成功获取锁，返回 true；否则返回 false。
             */
            bool try_lock() {
                return lock_.try_lock();
            }

            /**
             * @brief 尝试锁定互斥量，最多等待指定的时间。
             * 尝试锁定关联的互斥量，最多等待 `timeout` 指定的时间。
             * 此方法仅对定时互斥量类型 (std::timed_mutex, std::recursive_timed_mutex) 有效。
             * 对于其他互斥量类型，它会回退到调用 `try_lock`。
             *
             * @tparam Rep 表示时间间隔的算术类型。
             * @tparam Period 表示时间单位的 std::ratio 类型。
             * @param timeout 最大等待时间。
             * @return 如果成功获取锁，返回 true；如果超时或未能获取锁，返回 false。
             */
            template<typename Rep, typename Period>
            bool try_lock_for(const std::chrono::duration<Rep, Period> &timeout) {
                // 使用 if constexpr 在编译时根据 MutexType 选择不同的行为
                if constexpr (std::is_same_v<MutexType, std::timed_mutex> ||
                              std::is_same_v<MutexType, std::recursive_timed_mutex>) {
                    // 对于定时互斥量，调用其 try_lock_for 方法
                    return lock_.try_lock_for(timeout);
                } else {
                    // 对于非定时互斥量，只调用 try_lock
                    return lock_.try_lock();
                }
            }

            /**
             * @brief 检查 LockGuard 对象是否持有锁。
             *
             * @return 如果 LockGuard 对象当前持有其关联互斥量的锁，返回 true；否则返回 false。
             */
            bool owns_lock() const noexcept {
                return lock_.owns_lock();
            }

            // Note: LockGuard does not expose lock() or unlock() directly
            // as its purpose is RAII. Use std::unique_lock directly for manual control.

        private:
            /**
             * @brief 底层 std::unique_lock 对象。
             * 管理关联互斥量的锁定状态和所有权。
             */
            std::unique_lock<MutexType> lock_;
        };

    } // namespace LockManager
} // namespace LIBLSX

#endif // LIBLSX_LOCK_MANAGER_LOCK_GUARD_H
