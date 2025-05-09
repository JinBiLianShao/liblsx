/**
 * @file Queue.h
 * @brief 通用队列类 (模板)
 * @details 定义了 LIB_LSX::Memory 命名空间下的 Queue 类，
 * 这是一个模板类，用于实现一个线程安全的、基于 std::deque 的通用队列（先进先出）。
 * 提供非阻塞的数据放入（Push/Put）和取出（Pop/Get）操作。
 * 类内部使用 std::mutex 来保证在多线程环境下的线程安全访问。
 * 基于 std::deque，此实现通常是无界的（受限于系统内存）。
 * 类禁用了拷贝和赋值，以避免线程同步状态的复杂性。
 * @author 连思鑫（liansixin）
 * @date 2025-5-7
 * @version 1.0
 *
 * ### 核心功能
 * - **模板类**: 可存储任意类型 T 的元素。
 * - **先进先出 (FIFO)**: 保证元素的存取顺序。
 * - **无界容量**: 容量通常受限于系统内存，而非固定大小。
 * - **非阻塞操作**: 提供 `Push`/`Put` 和 `Pop`/`Get` 方法，在队列空时立即返回（对于 Pop/Get）。
 * - **线程安全**: 使用内部互斥锁 (`std::mutex`) 保护对队列的并发访问。
 * - **状态查询**: 提供 `IsEmpty`, `Size` 方法查询队列状态。
 * - **Peek 操作**: 支持查看队列头部的元素而不将其移除。
 * - **资源管理**: RAII 模式, `std::deque` 自动管理内存。
 *
 * ### 使用示例
 *
 * @code
 * #include "Queue.h"
 * #include <iostream>
 * #include <vector>
 * #include <thread>
 * #include <string>
 * #include <chrono>
 * #include <optional>
 *
 * // 创建一个存储整数的 Queue
 * LIB_LSX::Memory::Queue<int> int_queue;
 *
 * // 生产者线程
 * void int_producer(int id) {
 * for (int i = 0; i < 5; ++i) {
 * int value = i + id * 100;
 * std::cout << "[Int Producer " << id << "] Attempting to push value " << value << "..." << std::endl;
 * int_queue.Push(value); // 非阻塞 Push
 * std::cout << "[Int Producer " << id << "] Successfully pushed. Queue size: " << int_queue.Size() << std::endl;
 * std::this_thread::sleep_for(std::chrono::milliseconds(100)); // 短暂休息
 * }
 * }
 *
 * // 消费者线程
 * void int_consumer(int id) {
 * for (int i = 0; i < 5; ++i) {
 * std::cout << "[Int Consumer " << id << "] Attempting to pop..." << std::endl;
 * // 非阻塞 Pop, 如果队列空则返回 nullopt
 * std::optional<int> value = int_queue.Pop();
 * if (value) {
 * std::cout << "[Int Consumer " << id << "] Successfully popped value " << *value << ". Queue size: " << int_queue.Size() << std::endl;
 * } else {
 * std::cerr << "[Int Consumer " << id << "] Failed to pop (queue empty)." << std::endl;
 * }
 * std::this_thread::sleep_for(std::chrono::milliseconds(250)); // 模拟处理时间
 * }
 * }
 *
 * int main() {
 * std::cout << "Int Queue created." << std::endl;
 *
 * std::thread p1(int_producer, 1);
 * std::thread c1(int_consumer, 1);
 *
 * p1.join();
 * c1.join();
 *
 * std::cout << "All threads finished." << std::endl;
 * std::cout << "Final queue size: " << int_queue.Size() << std::endl;
 *
 * // 示例：使用 Peek
 * if (!int_queue.IsEmpty()) {
 * try {
 * const int& peeked_value = int_queue.Peek();
 * std::cout << "Peeked at front value: " << peeked_value << std::endl;
 * } catch (const std::out_of_range& e) {
 * std::cerr << "Peek failed: " << e.what() << std::endl;
 * }
 * }
 *
 * // 示例：清空队列
 * int_queue.Clear();
 * std::cout << "Queue cleared. IsEmpty: " << int_queue.IsEmpty() << std::endl;
 *
 * std::cout << "主线程退出。" << std::endl;
 * return 0;
 * }
 * @endcode
 *
 * ### 注意事项
 * - **无界队列**: 基于 std::deque, Queue 队列通常是无界的。如果需要固定容量或阻塞行为，应使用 CircularQueue 或专门的阻塞队列实现。
 * - **非阻塞**: `Push`/`Put` 和 `Pop`/`Get` 方法是非阻塞的。`Push`/`Put` 总是成功（除非内存耗尽），`Pop`/`Get` 在队列空时返回 std::nullopt。
 * - **线程安全**: 所有公共方法都通过互斥锁保护，支持多线程访问。
 * - **数据复制/移动**: `Push`/`Put` 涉及元素的复制或移动（取决于 T 的类型）。`Pop`/`Get` 返回元素的拷贝或移动后的值。`Peek` 返回常量引用。
 * - **异常处理**: `Peek` 在队列为空时抛出 `std::out_of_range` 异常。`Pop`/`Get` 在队列空时返回 std::nullopt。
 * - **拷贝/移动**: 类禁用了拷贝构造和赋值，因为直接拷贝一个包含线程同步原语（mutex）和状态的队列通常是不安全或没有意义的。如果需要传递队列对象，应考虑使用智能指针（如 `std::shared_ptr`）。移动语义未默认启用，如果需要，可以手动实现。
 */

#ifndef LIB_LSX_MEMORY_QUEUE_H
#define LIB_LSX_MEMORY_QUEUE_H
#pragma once
#include <deque> // For std::deque
#include <optional> // For std::optional (C++17)
#include <stdexcept> // For exceptions (std::out_of_range)
#include <mutex>    // For thread safety (std::mutex, std::lock_guard)
#include <condition_variable> // For potential blocking operations (std::condition_variable) - commented out in provided code
#include <utility> // For std::move
// #include <iostream> // For example output - prefer logging


/**
 * @brief LSX 库的根命名空间。
 */
namespace LIB_LSX {
    /**
     * @brief 内存管理相关的命名空间。
     * 包含内存缓冲区和相关工具。
     */
    namespace Memory {

        /**
         * @brief 通用队列类 (模板)。
         * 实现一个线程安全的、基于 std::deque 的通用队列（先进先出），可存储任意类型 T 的元素。
         * 提供非阻塞的放入和取出操作。通常是无界的。
         * @tparam T 队列中存储的元素类型。
         */
        // 4. Queue Module (队列模块)
        // 实现通用队列功能 (模板类)
        // 接口优化：增加Clear, Peek, Put/Get别名
        // 线程安全：使用 std::mutex
        template<typename T>
        class Queue {
        private:
            /**
             * @brief 存储队列数据的底层 std::deque。
             * 提供了高效的双端操作，std::queue 默认使用它作为底层容器。
             */
            std::deque<T> data_deque_; // std::queue uses std::deque by default
            /**
             * @brief 互斥锁。
             * 用于保护 data_deque_ 的并发访问，确保线程安全。
             */
            mutable std::mutex mutex_; // Thread safety mutex
            // std::condition_variable cv_read_; // For potential blocking operations (commented out in provided code)
            // std::condition_variable cv_write_; // For potential blocking operations (commented out in provided code)


        public:
            // Constructor/Destructor
            /**
             * @brief 默认构造函数。
             * 创建一个空的 Queue。
             */
            Queue() = default;
            /**
             * @brief 析构函数。
             * 默认析构函数，由 std::deque 自动管理底层内存释放。
             */
            ~Queue() = default;

            // Prevent copying and assignment for simplicity, or implement properly
            /**
             * @brief 禁用拷贝构造函数。
             * Queue 包含同步原语，拷贝不安全。
             */
            Queue(const Queue&) = delete;
            /**
             * @brief 禁用拷贝赋值运算符。
             * Queue 包含同步原语，拷贝不安全。
             */
            Queue& operator=(const Queue&) = delete;
            // Consider adding move constructor/assignment if state transfer is safe

            // --- Management Functions ---
            /**
             * @brief 清空队列。
             * 移除队列中的所有元素。
             */
            // Clear the queue
            void Clear() {
                std::lock_guard<std::mutex> lock(mutex_); // Thread safe clear
                data_deque_.clear(); // std::deque::clear()
                // std::cout << "Queue: Cleared." << std::endl; // Use logging
                // cv_write_.notify_all(); // Notify potential waiting writers (if blocking was enabled)
                // cv_read_.notify_all(); // Notify potential waiting readers (if blocking was enabled)
            }

            // --- Data Access Functions ---
            /**
             * @brief 将一个元素添加到队列尾部 (非阻塞)。
             * 将指定元素复制或移动到队列的末尾。
             * 此操作总是成功，除非内存耗尽。
             *
             * @param value 要放入的元素。
             */
            // Add an element to the back (Enqueue / Put)
            void Push(const T& value) {
                std::lock_guard<std::mutex> lock(mutex_); // Thread safe push
                data_deque_.push_back(value);
                // std::cout << "Queue: Pushed value." << std::endl; // Use logging
                // cv_read_.notify_one(); // Notify potential waiting readers (if blocking was enabled)
            }

            /**
             * @brief 将一个元素添加到队列尾部 (非阻塞)。
             * 便利方法，别名 Push。
             *
             * @param value 要放入的元素。
             */
            // Alias for Push
            void Put(const T& value) { Push(value); }


            /**
             * @brief 从队列头部移除并返回元素 (非阻塞)。
             * 将队列头部的元素移动或复制出来，并从队列中移除该元素。
             * 如果队列为空，立即返回 std::optional<T> 的空值 (std::nullopt)。
             *
             * @return 一个包含取出元素的 std::optional<T>。如果队列为空，返回 std::nullopt。
             */
            // Remove and return the element from the front (Dequeue / Get)
            std::optional<T> Pop() {
                std::lock_guard<std::mutex> lock(mutex_); // Thread safe pop
                if (data_deque_.empty()) {
                    // std::cout << "Queue: Queue is empty, cannot pop." << std::endl; // Use logging
                    return std::nullopt; // Use std::optional to indicate failure
                }
                T value = std::move(data_deque_.front()); // Use move for efficiency if T supports it
                data_deque_.pop_front();
                // std::cout << "Queue: Popped value." << std::endl; // Use logging
                // cv_write_.notify_one(); // Notify potential waiting writers (if blocking was enabled)
                return value;
            }

            /**
             * @brief 从队列头部移除并返回元素 (非阻塞)。
             * 便利方法，别名 Pop。
             *
             * @return 一个包含取出元素的 std::optional<T>。如果队列为空，返回 std::nullopt。
             */
            // Alias for Pop
            std::optional<T> Get() { return Pop(); }

            /**
             * @brief 查看队列头部的元素，但不移除 (非阻塞)。
             * 返回队列头部元素的常量引用。队列保持不变。
             * 如果队列为空，抛出 std::out_of_range 异常。
             *
             * @return 队列头部元素的常量引用。
             * @throws std::out_of_range 如果队列为空。
             */
            // Get the element at the front without removing it (Peek)
            const T& Peek() const {
                std::lock_guard<std::mutex> lock(mutex_); // Thread safe peek
                if (data_deque_.empty()) {
                    throw std::out_of_range("Queue: Queue is empty, cannot peek");
                }
                return data_deque_.front();
            }


            // --- Status Functions ---
            /**
             * @brief 检查队列是否为空。
             *
             * @return 如果队列中当前没有元素，返回 true；否则返回 false。
             */
            // Check if the queue is empty
            bool IsEmpty() const {
                std::lock_guard<std::mutex> lock(mutex_);
                return data_deque_.empty();
            }

            /**
             * @brief 获取队列中当前存储的元素数量。
             *
             * @return 队列中当前存储的元素数量。
             */
            // Get the number of elements in the queue
            size_t Size() const {
                std::lock_guard<std::mutex> lock(mutex_);
                return data_deque_.size();
            }

            // Queue is typically unbounded (std::deque). Capacity is conceptual infinite.
            // size_t Capacity() const; // Not applicable for unbounded queue
            // bool IsFull() const; // Not applicable for unbounded queue (unless memory is exhausted)
        };

    } // namespace Memory
} // namespace LIB_LSX

#endif // LIB_LSX_MEMORY_QUEUE_H
