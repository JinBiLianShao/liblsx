/**
 * @file Condition.h
 * @brief 条件变量封装类
 * @details 定义了 LIBLSX::LockManager 命名空间下的 Condition 类，
 * 这是一个对 std::condition_variable_any 的封装，提供了线程同步的功能。
 * 它允许线程在某个条件不满足时等待，并在条件满足时被通知唤醒。
 * 支持与满足 BasicLockable 要求的任何锁类型一起使用。
 * @author 连思鑫（liansixin）
 * @date 2025年5月9日
 * @version 1.0
 *
 * ### 核心功能
 * - **条件等待**: 提供 `wait` 方法，使线程在持有锁的情况下等待特定条件。
 * - **谓词等待**: 支持带谓词的 `wait`，避免虚假唤醒。
 * - **通知机制**: 提供 `notify_one` 和 `notify_all` 方法，用于唤醒等待的线程。
 * - **通用锁支持**: 使用 `std::condition_variable_any`，可与任何满足 BasicLockable 要求的锁类型配合使用。
 * - **线程安全**: 类本身通过底层 std::condition_variable_any 保证线程安全。
 *
 * ### 使用示例
 *
 * @code
 * #include "Condition.h" // 包含 Condition 类定义
 * #include <iostream>
 * #include <thread>
 * #include <mutex>
 * #include <vector>
 *
 * namespace LIBLSX {
 * namespace LockManager {
 * // Forward declaration for example usage
 * class Condition;
 * }
 * }
 *
 * std::mutex g_mutex; // 用于保护共享数据和条件变量
 * LIBLSX::LockManager::Condition g_condition; // 条件变量实例
 * bool g_data_ready = false; // 共享数据状态标志
 * int g_shared_value = 0;
 *
 * // 生产者线程
 * void producer_thread() {
 * std::this_thread::sleep_for(std::chrono::seconds(1)); // 模拟生产数据的时间
 * {
 * std::lock_guard<std::mutex> lock(g_mutex); // 锁定共享数据
 * g_shared_value = 100;
 * g_data_ready = true; // 数据已准备好
 * std::cout << "[Producer] Data is ready. Value: " << g_shared_value << std::endl;
 * } // 锁在此处释放
 *
 * g_condition.notify_one(); // 通知一个等待的消费者线程
 * std::cout << "[Producer] Notified one consumer." << std::endl;
 *
 * std::this_thread::sleep_for(std::chrono::seconds(1)); // 模拟更多工作
 * {
 * std::lock_guard<std::mutex> lock(g_mutex);
 * g_shared_value = 200;
 * g_data_ready = true; // 数据再次准备好
 * std::cout << "[Producer] Data is ready again. Value: " << g_shared_value << std::endl;
 * }
 * g_condition.notify_all(); // 通知所有等待的消费者线程
 * std::cout << "[Producer] Notified all consumers." << std::endl;
 * }
 *
 * // 消费者线程
 * void consumer_thread(int id) {
 * std::cout << "[Consumer " << id << "] Waiting for data..." << std::endl;
 * {
 * std::unique_lock<std::mutex> lock(g_mutex); // 使用 unique_lock 以便 wait 期间解锁
 *
 * // 等待条件：g_data_ready 为 true
 * g_condition.wait(lock, [&]{
 * std::cout << "[Consumer " << id << "] Checking condition..." << std::endl;
 * return g_data_ready;
 * });
 *
 * // 条件满足，数据可用
 * std::cout << "[Consumer " << id << "] Data received. Value: " << g_shared_value << std::endl;
 * g_data_ready = false; // 重置标志，表示已消费
 * } // 锁在此处释放
 *
 * std::cout << "[Consumer " << id << "] Finished processing." << std::endl;
 * }
 *
 * int main() {
 * std::thread producer_t(producer_thread);
 * std::thread consumer1_t(consumer_thread, 1);
 * std::thread consumer2_t(consumer_thread, 2);
 *
 * producer_t.join();
 * consumer1_t.join();
 * consumer2_t.join();
 *
 * std::cout << "主线程退出。" << std::endl;
 * return 0;
 * }
 * @endcode
 *
 * ### 注意事项
 * - **锁类型**: `wait` 方法是模板方法，可以接受任何满足 BasicLockable 要求的锁类型（如 `std::unique_lock`, `std::lock_guard`，但通常推荐 `std::unique_lock` 因为 `wait` 需要临时释放锁）。
 * - **谓词**: 使用带谓词的 `wait` 是推荐的做法，可以避免虚假唤醒（spurious wakeups）。谓词是一个可调用对象，在 `wait` 返回前被检查。
 * - **虚假唤醒**: 即使没有调用 `notify_one` 或 `notify_all`，等待的线程也可能被唤醒（虚假唤醒）。因此，在 `wait` 返回后，总是需要重新检查条件是否真的满足。使用带谓词的 `wait` 可以自动处理这个检查。
 * - **通知时机**: 通常在修改了等待线程所依赖的共享数据后，并且在释放保护共享数据的锁之前，调用 `notify_one` 或 `notify_all`。
 * - **头文件实现**: 由于 `wait` 方法是模板方法，其实现必须放在头文件中。`notify_one` 和 `notify_all` 如果不是模板，可以放在 .cpp 文件中实现。
 */

#ifndef LIBLSX_LOCK_MANAGER_CONDITION_H
#define LIBLSX_LOCK_MANAGER_CONDITION_H
#pragma once

#include <condition_variable> // For std::condition_variable_any
#include <mutex> // For std::unique_lock, std::lock_guard (used in examples and concepts)
#include <chrono> // For std::chrono::duration (used in potential timed waits)
#include <utility> // For std::forward (used in template perfect forwarding)
#include <functional> // For std::function (used in predicate)


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
         * @brief 条件变量封装类。
         * 对 std::condition_variable_any 的封装，提供线程同步功能。
         * 允许线程在某个条件不满足时等待，并在条件满足时被通知唤醒。
         */
        class Condition {
        public:
            /**
             * @brief 等待条件（无谓词）。
             * 当前线程原子地释放指定的锁，并阻塞等待条件变量被通知。
             * 当被通知（或发生虚假唤醒）时，线程会被唤醒，并在 `wait` 返回前重新获取锁。
             * LockType 必须满足 BasicLockable 要求。此方法是模板方法，实现必须在头文件中。
             *
             * @tparam LockType 满足 BasicLockable 要求的锁类型。
             * @param lock 当前线程持有的锁对象。
             */
            // 等待条件（无谓词）。
            // LockType 必须满足 BasicLockable 要求。作为模板方法，其实现必须在头文件中。
            template <typename LockType>
            void wait(LockType& lock) {
                cv_.wait(lock);
            }

            /**
             * @brief 等待条件（带谓词），直到谓词返回 true。
             * 当前线程原子地释放指定的锁，并阻塞等待条件变量被通知。
             * 当被通知（或发生虚假唤醒）时，线程会被唤醒，并在 `wait` 返回前重新获取锁。
             * 在每次唤醒后，会检查谓词 `pred()`。只有当谓词返回 true 时，`wait` 才会返回。
             * LockType 必须满足 BasicLockable 要求。Predicate 是一个可调用对象，返回 bool。
             * 此方法是模板方法，实现必须在头文件中。
             *
             * @tparam LockType 满足 BasicLockable 要求的锁类型。
             * @tparam Predicate 返回 bool 的可调用对象类型。
             * @param lock 当前线程持有的锁对象。
             * @param pred 谓词函数或可调用对象。
             */
            // 等待条件（带谓词），直到 pred() 返回 true。
            // LockType 必须满足 BasicLockable 要求。作为模板方法，其实现必须在头文件中。
            template <typename LockType, typename Predicate>
            void wait(LockType& lock, Predicate pred) {
                cv_.wait(lock, std::forward<Predicate>(pred)); // Use std::forward for perfect forwarding
            }

            /**
             * @brief 通知一个等待的线程。
             * 唤醒一个（如果存在）正在等待此条件变量的线程。如果多个线程正在等待，唤醒哪个线程是不确定的。
             * 通常在修改了等待线程依赖的条件后调用。
             *
             * @note 此方法通常在释放保护共享数据的锁之前调用。
             */
            // 通知一个等待的线程。声明，实现将在 .cpp 文件中。
            void notify_one() noexcept;

            /**
             * @brief 通知所有等待的线程。
             * 唤醒所有（如果存在）正在等待此条件变量的线程。
             * 通常在修改了等待线程依赖的条件后调用。
             *
             * @note 此方法通常在释放保护共享数据的锁之前调用。
             */
            // 通知所有等待的线程。声明，实现将在 .cpp 文件中。
            void notify_all() noexcept;


        private:
            /**
             * @brief 底层条件变量对象。
             * std::condition_variable_any 可以与任何满足 BasicLockable 要求的锁类型一起使用。
             */
            std::condition_variable_any cv_;
        };

    } // namespace LockManager
} // namespace LIBLSX

#endif // LIBLSX_LOCK_MANAGER_CONDITION_H
