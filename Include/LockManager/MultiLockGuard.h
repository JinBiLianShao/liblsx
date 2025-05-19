/**
 * @file MultiLockGuard.h
 * @brief 多互斥量锁守卫类 (模板)
 * @details 定义了 LIBLSX::LockManager 命名空间下的 MultiLockGuard 类，
 * 这是一个模板类，用于实现 RAII (Resource Acquisition Is Initialization) 风格的多个互斥量锁管理。
 * 它内部使用 std::scoped_lock 封装了可变数量的互斥量，
 * 确保在 MultiLockGuard 对象构造时原子地锁定所有指定的互斥量，并在对象超出作用域时自动释放它们。
 * 使用 std::scoped_lock 可以避免死锁，因为它会按照特定的顺序尝试锁定互斥量。
 * 类禁用了拷贝和赋值，以确保锁的所有权不会被意外转移或复制。
 * @author 连思鑫（liansixin）
 * @date 2025年5月9日
 * @version 1.0
 *
 * ### 核心功能
 * - **RAII 多锁管理**: 自动在构造时原子地获取多个锁，在析构时释放它们。
 * - **死锁避免**: 使用 std::scoped_lock 的机制来避免锁定多个互斥量时可能发生的死锁。
 * - **通用互斥量支持**: 可与 std::mutex, std::recursive_mutex, std::timed_mutex, std::recursive_timed_mutex 等满足 BasicLockable 要求的互斥量类型配合使用。
 * - **线程安全**: 类本身通过正确使用 std::scoped_lock 来管理底层互斥量，确保线程安全。
 *
 * ### 使用示例
 *
 * @code
 * #include "MultiLockGuard.h" // 包含 MultiLockGuard 类定义
 * #include <iostream>
 * #include <thread>
 * #include <mutex>
 * #include <vector>
 * #include <chrono>
 *
 * namespace LIBLSX {
 * namespace LockManager {
 * // Forward declaration for example usage
 * template <typename... Mutexes> class MultiLockGuard;
 * }
 * }
 *
 * std::mutex mutex1;
 * std::mutex mutex2;
 * std::mutex mutex3;
 *
 * // 共享资源，受 mutex1, mutex2, mutex3 保护
 * int shared_data1 = 0;
 * int shared_data2 = 0;
 * int shared_data3 = 0;
 *
 * // 示例：使用 MultiLockGuard 同时锁定多个互斥量
 * void access_multiple_resources(int id) {
 * std::cout << "[Thread " << id << "] Attempting to acquire multiple mutexes..." << std::endl;
 *
 * // 使用 MultiLockGuard 原子地锁定 mutex1, mutex2, mutex3
 * LIBLSX::LockManager::MultiLockGuard<std::mutex, std::mutex, std::mutex> lock(mutex1, mutex2, mutex3);
 *
 * std::cout << "[Thread " << id << "] All mutexes acquired." << std::endl;
 *
 * // 访问共享资源
 * shared_data1++;
 * shared_data2 += 2;
 * shared_data3 += 3;
 * std::cout << "[Thread " << id << "] Shared resources updated: "
 * << shared_data1 << ", " << shared_data2 << ", " << shared_data3 << std::endl;
 *
 * std::this_thread::sleep_for(std::chrono::milliseconds(50)); // 模拟工作
 *
 * std::cout << "[Thread " << id << "] Releasing all mutexes." << std::endl;
 * } // MultiLockGuard 对象超出作用域，析构时自动释放所有锁
 *
 * int main() {
 * std::vector<std::thread> threads;
 * for (int i = 0; i < 10; ++i) {
 * threads.emplace_back(access_multiple_resources, i);
 * }
 *
 * for (auto& t : threads) {
 * t.join();
 * }
 *
 * std::cout << "All threads finished." << std::endl;
 * std::cout << "Final shared data: "
 * << shared_data1 << ", " << shared_data2 << ", " << shared_data3 << std::endl;
 *
 * std::cout << "主线程退出。" << std::endl;
 * return 0;
 * }
 * @endcode
 *
 * ### 注意事项
 * - **锁类型要求**: MultiLockGuard 要求所有 MutexType 参数都满足 BasicLockable 要求。
 * - **原子锁定**: 构造函数会尝试原子地锁定所有指定的互斥量。如果锁定过程中发生异常，已获取的锁会被正确释放。
 * - **死锁避免**: std::scoped_lock 内部实现了死锁避免算法（例如，通过 std::lock 结合锁顺序）。
 * - **不可拷贝/赋值**: MultiLockGuard 对象不可拷贝或赋值，以防止多个对象管理同一组互斥量，导致未定义行为。
 * - **所有权**: MultiLockGuard 在构造时获取所有指定锁的所有权，并在析构时释放。确保 MultiLockGuard 对象的生命周期与需要保护的临界区范围一致。
 * - **无延迟锁定/尝试锁定**: MultiLockGuard (基于 std::scoped_lock) 不支持延迟锁定或单独的 try_lock/try_lock_for 方法。它总是尝试在构造时一次性锁定所有互斥量。
 */

#ifndef LIB_LSX_LOCK_MANAGER_MULTI_LOCK_GUARD_H
#define LIB_LSX_LOCK_MANAGER_MULTI_LOCK_GUARD_H
#pragma once

#include <mutex> // For std::scoped_lock, std::mutex etc.
#include <utility> // For std::forward (if needed in template methods)


/**
 * @brief LSX 库的根命名空间。
 */
namespace LIB_LSX {
    /**
     * @brief 锁管理相关的命名空间。
     * 包含锁和同步原语的封装。
     */
    namespace LockManager {

        /**
         * @brief 多互斥量锁守卫类 (模板)。
         * 实现 RAII 风格的多个互斥量锁管理。
         * 封装 std::scoped_lock，确保在对象构造时原子地锁定可变数量的互斥量，并在超出作用域时自动释放。
         * 使用 std::scoped_lock 可以避免死锁。
         * @tparam Mutexes 可变参数模板，表示要锁定的互斥量类型列表。每个类型都必须满足 BasicLockable 要求。
         */
        template <typename... Mutexes>
        class MultiLockGuard {
        public:
            /**
             * @brief 构造函数。
             * 构造一个 MultiLockGuard 对象，并尝试原子地锁定所有指定的互斥量 `ms`。
             * 如果锁定失败，会阻塞直到获取所有锁。使用 std::scoped_lock 内部的死锁避免算法。
             *
             * @param ms 要锁定的可变数量的互斥量引用。
             */
            explicit MultiLockGuard(Mutexes&... ms)
                : lock_(ms...) {}

            /**
             * @brief 禁用拷贝构造函数。
             * MultiLockGuard 对象管理锁的所有权，不可拷贝。
             */
            MultiLockGuard(const MultiLockGuard&) = delete;
            /**
             * @brief 禁用赋值运算符。
             * MultiLockGuard 对象管理锁的所有权，不可赋值。
             */
            MultiLockGuard& operator=(const MultiLockGuard&) = delete;

            /**
             * @brief 析构函数。
             * 在 MultiLockGuard 对象超出作用域时自动调用。
             * 析构函数会自动释放构造时成功获取的所有互斥量。
             */
            ~MultiLockGuard() noexcept = default;

            // Note: MultiLockGuard (based on std::scoped_lock) does not expose
            // individual lock/unlock, try_lock, try_lock_for, or owns_lock methods
            // as it manages all locks atomically as a group.

        private:
            /**
             * @brief 底层 std::scoped_lock 对象。
             * 管理关联互斥量的锁定状态和所有权。
             * 在构造时锁定所有互斥量，在析构时释放。
             */
            std::scoped_lock<Mutexes...> lock_;
        };

    } // namespace LockManager
} // namespace LIBLSX

#endif

