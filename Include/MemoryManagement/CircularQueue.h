/**
 * @file CircularQueue.h
 * @brief 循环队列类 (模板)
 * @details 定义了 LIB_LSX::Memory 命名空间下的 CircularQueue 类，
 * 这是一个模板类，用于实现一个线程安全的、固定可用容量的循环队列。
 * 队列基于一个内部的 std::vector，利用头部和尾部索引实现循环存储。
 * 提供非阻塞的数据放入（Enqueue/Put）和取出（Dequeue/Get）操作，
 * 并使用 std::mutex 来保证在多线程环境下的线程安全访问。
 * 类禁用了拷贝和赋值，以避免线程同步状态的复杂性。
 * @author 连思鑫（liansixin）
 * @date 2025-5-7
 * @version 1.0
 *
 * ### 核心功能
 * - **模板类**: 可存储任意类型 T 的元素。
 * - **固定可用容量**: 在构造时指定队列的最大可用元素数量。
 * - **循环队列**: 利用底层线性缓冲区模拟循环队列的行为，高效利用内存。
 * - **非阻塞操作**: 提供 `Enqueue`/`Put` 和 `Dequeue`/`Get` 方法，在队列满或空时立即返回。
 * - **线程安全**: 使用内部互斥锁 (`std::mutex`) 保护对队列状态和底层缓冲区的并发访问。
 * - **状态查询**: 提供 `IsEmpty`, `IsFull`, `Size`, `Capacity` 方法查询队列状态和属性。
 * - **Peek 操作**: 支持查看队列头部的元素而不将其移除。
 * - **资源管理**: RAII 模式，`std::vector` 自动管理底层内存。
 *
 * ### 使用示例
 *
 * @code
 * #include "CircularQueue.h"
 * #include <iostream>
 * #include <vector>
 * #include <thread>
 * #include <string>
 * #include <chrono>
 *
 * // 创建一个可用容量为 5 的整数循环队列
 * LIB_LSX::Memory::CircularQueue<int> int_queue(5);
 *
 * // 生产者线程
 * void int_producer(int id) {
 * for (int i = 0; i < 10; ++i) {
 * int value = i + id * 100;
 * std::cout << "[Int Producer " << id << "] Attempting to put value " << value << "..." << std::endl;
 * // 非阻塞 Put，如果队列满则失败
 * if (int_queue.Put(value)) {
 * std::cout << "[Int Producer " << id << "] Successfully put value " << value << ". Queue size: " << int_queue.Size() << std::endl;
 * } else {
 * std::cerr << "[Int Producer " << id << "] Failed to put value " << value << " (queue full)." << std::endl;
 * }
 * std::this_thread::sleep_for(std::chrono::milliseconds(50)); // 短暂休息
 * }
 * }
 *
 * // 消费者线程
 * void int_consumer(int id) {
 * for (int i = 0; i < 10; ++i) {
 * std::cout << "[Int Consumer " << id << "] Attempting to get value " << i << "..." << std::endl;
 * // 非阻塞 Get，如果队列空则返回 nullopt
 * std::optional<int> value = int_queue.Get();
 * if (value) {
 * std::cout << "[Int Consumer " << id << "] Successfully got value " << *value << ". Queue size: " << int_queue.Size() << std::endl;
 * } else {
 * std::cerr << "[Int Consumer " << id << "] Failed to get value " << i << " (queue empty)." << std::endl;
 * }
 * std::this_thread::sleep_for(std::chrono::milliseconds(150)); // 模拟处理时间
 * }
 * }
 *
 * int main() {
 * std::cout << "Int Queue created with usable capacity " << int_queue.Capacity() << std::endl;
 *
 * std::thread p1(int_producer, 1);
 * std::thread c1(int_consumer, 1);
 * std::thread p2(int_producer, 2);
 * std::thread c2(int_consumer, 2);
 *
 * p1.join();
 * c1.join();
 * p2.join();
 * c2.join();
 *
 * std::cout << "All threads finished." << std::endl;
 * std::cout << "Final queue size: " << int_queue.Size() << std::endl;
 *
 * // 示例：使用 Peek
 * if (!int_queue.IsEmpty()) {
 * const int& peeked_value = int_queue.Peek();
 * std::cout << "Peeked at front value: " << peeked_value << std::endl;
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
 * - **固定可用容量**: 队列创建后，其可用容量是固定的。底层 vector 的实际大小是可用容量加一，用于区分满和空的状态。
 * - **非阻塞**: `Enqueue`/`Put` 和 `Dequeue`/`Get` 方法是非阻塞的。在队列满或空时会立即返回，而不是等待。如果需要阻塞行为，可以考虑在此基础上使用条件变量或使用专门的阻塞队列实现。
 * - **线程安全**: 所有公共方法都通过互斥锁保护，支持多线程访问。
 * - **数据复制/移动**: `Enqueue`/`Put` 涉及元素的复制或移动（取决于 T 的类型）。`Dequeue`/`Get` 返回元素的拷贝或移动后的值。`Peek` 返回常量引用。
 * - **异常处理**: 构造函数可能因容量为 0 抛出 `std::invalid_argument`，或因内存分配失败抛出 `std::bad_alloc`。`Peek` 在队列为空时抛出 `std::out_of_range`。其他非阻塞操作在失败时返回 false 或 std::nullopt。
 * - **拷贝/移动**: 类禁用了拷贝构造和赋值，因为直接拷贝一个包含线程同步原语（mutex）和状态（head, tail, size）的队列通常是不安全或没有意义的。如果需要传递队列对象，应考虑使用智能指针（如 `std::shared_ptr`）。移动语义已默认启用。
 * - **底层实现细节**: 底层 vector 的大小比可用容量大 1 是循环队列的标准实现技巧，用于区分队列满和空的状态。
 */

#ifndef LIB_LSX_MEMORY_CIRCULAR_QUEUE_H
#define LIB_LSX_MEMORY_CIRCULAR_QUEUE_H
#pragma once
// 包含全局错误锁头文件
#include "GlobalErrorMutex.h"
// 包含 LIBLSX::LockManager::LockGuard 头文件
#include "LockGuard.h"
#include <vector> // For std::vector
#include <optional> // For std::optional (C++17)
#include <stdexcept> // For exceptions (std::invalid_argument, std::out_of_range)
#include <algorithm> // For std::min (not directly used in provided code, but common)
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
         * @brief 循环队列类 (模板)。
         * 实现一个线程安全的、固定可用容量的循环队列，可存储任意类型 T 的元素。
         * 提供非阻塞的放入和取出操作。
         * @tparam T 队列中存储的元素类型。
         */
        // 3. CircularQueue Module (循环队列模块)
        // 实现循环利用存储空间的队列 (模板类)
        // 接口优化：增加Clear, Peek, Put/Get别名
        // 线程安全：使用 std::mutex
        template<typename T>
        class CircularQueue {
        private:
            /**
             * @brief 存储队列数据的底层 vector。
             * 实际大小为 capacity_，等于可用容量加一。
             */
            std::vector<T> data_vector_;
            /**
             * @brief 底层 vector 的实际大小。
             * 等于可用容量加一，用于区分队列满和空的状态。
             */
            size_t capacity_; // Actual allocated size (usable capacity + 1)
            /**
             * @brief 队列头部索引。
             * 指向队列中最旧的有效元素的索引。
             */
            size_t head_ = 0; // Index of the front element
            /**
             * @brief 队列尾部索引。
             * 指向下一个新元素将要插入的位置的索引。
             */
            size_t tail_ = 0; // Index where the next element will be inserted
            /**
             * @brief 队列中当前存储的元素数量。
             */
            size_t current_size_ = 0;
            /**
             * @brief 互斥锁。
             * 用于保护队列的状态变量 (head_, tail_, current_size_) 和底层缓冲区 (data_vector_) 的并发访问。
             */
            mutable std::mutex mutex_; // Thread safety mutex
            // std::condition_variable cv_read_; // For potential blocking operations (commented out in provided code)
            // std::condition_variable cv_write_; // For potential blocking operations (commented out in provided code)


        public:
            /**
             * @brief 构造函数。
             * 初始化循环队列，分配底层 vector 缓冲区。
             *
             * @param usable_capacity 队列的最大可用元素数量。必须大于 0。
             * @throws std::invalid_argument 如果 usable_capacity 为 0。
             * @throws std::bad_alloc 如果底层 vector 内存分配失败。
             */
            // Constructor with fixed capacity
            // Note: The constructor argument is the usable capacity.
            CircularQueue(size_t usable_capacity) : capacity_(usable_capacity + 1) { // Use capacity+1 to distinguish full/empty
                if (usable_capacity == 0) {
                    throw std::invalid_argument("CircularQueue usable capacity must be greater than 0");
                }
                // Lock during construction if shared state could be accessed before constructor finishes
                // No need for lock here as no other threads can access the object during construction
                data_vector_.resize(capacity_);
                // std::cout << "CircularQueue: Created with usable capacity " << usable_capacity << std::endl; // Use logging
            }

            /**
             * @brief 析构函数。
             * 默认析构函数，由 std::vector 自动管理底层内存释放。
             */
            ~CircularQueue() = default;

            // Prevent copying and assignment
            /**
             * @brief 禁用拷贝构造函数。
             * 循环队列包含同步原语和状态，拷贝不安全。
             */
            CircularQueue(const CircularQueue&) = delete;
            /**
             * @brief 禁用拷贝赋值运算符。
             * 循环队列包含同步原语和状态，拷贝不安全。
             */
            CircularQueue& operator=(const CircularQueue&) = delete;
            // Consider adding move constructor/assignment if state transfer is safe

            // --- Management Functions ---
            /**
             * @brief 清空队列。
             * 重置队列的头部、尾部和当前大小，但不擦除底层数据。
             * 使队列回到初始的空状态。
             */
            void Clear() {
                std::lock_guard<std::mutex> lock(mutex_); // Thread safe clear
                head_ = 0;
                tail_ = 0;
                current_size_ = 0;
                // Data is not erased from vector, just logically reset pointers
                // std::cout << "CircularQueue: Cleared." << std::endl; // Use logging
                // cv_write_.notify_all(); // Notify potential waiting writers (if blocking was enabled)
                // cv_read_.notify_all(); // Notify potential waiting readers (if blocking was enabled)
            }

            // --- Data Access Functions (Non-blocking) ---
            /**
             * @brief 将一个元素添加到队列尾部 (非阻塞)。
             * 将指定元素复制或移动到队列尾部的下一个可用位置。
             * 如果队列已满，立即返回 false。
             *
             * @param value 要放入的元素。
             * @return 如果成功放入元素，返回 true；如果队列已满，返回 false。
             */
            // Add an element to the back (Enqueue / Put)
            // Returns true on success, false if full
            bool Enqueue(const T& value) {
                std::lock_guard<std::mutex> lock(mutex_); // Thread safe enqueue
                if (IsFull()) {
                    // std::cout << "CircularQueue: Queue is full, cannot enqueue." << std::endl; // Use logging
                    return false;
                }
                data_vector_[tail_] = value; // Copy assignment
                tail_ = (tail_ + 1) % capacity_;
                current_size_++;
                // std::cout << "CircularQueue: Enqueued value." << std::endl; // Use logging
                // cv_read_.notify_one(); // Notify potential waiting readers (if blocking was enabled)
                return true;
            }

            /**
             * @brief 将一个元素添加到队列尾部 (非阻塞)。
             * 便利方法，别名 Enqueue。
             *
             * @param value 要放入的元素。
             * @return 如果成功放入元素，返回 true；如果队列已满，返回 false。
             */
            // Alias for Enqueue
            bool Put(const T& value) { return Enqueue(value); }


            /**
             * @brief 从队列头部移除并返回元素 (非阻塞)。
             * 将队列头部的元素移动或复制出来，并从队列中移除该元素。
             * 如果队列为空，立即返回 std::nullopt。
             *
             * @return 一个包含取出元素的 std::optional<T>。如果队列为空，返回 std::nullopt。
             */
            // Remove and return the element from the front (Dequeue / Get)
            std::optional<T> Dequeue() {
                std::lock_guard<std::mutex> lock(mutex_); // Thread safe dequeue
                if (IsEmpty()) {
                    // std::cout << "CircularQueue: Queue is empty, cannot dequeue." << std::endl; // Use logging
                    return std::nullopt; // Use std::optional
                }
                T value = std::move(data_vector_[head_]); // Use move to potentially improve performance
                head_ = (head_ + 1) % capacity_;
                current_size_--;
                // std::cout << "CircularQueue: Dequeued value." << std::endl; // Use logging
                // cv_write_.notify_one(); // Notify potential waiting writers (if blocking was enabled)
                return value;
            }

            /**
             * @brief 从队列头部移除并返回元素 (非阻塞)。
             * 便利方法，别名 Dequeue。
             *
             * @return 一个包含取出元素的 std::optional<T>。如果队列为空，返回 std::nullopt。
             */
            // Alias for Dequeue
            std::optional<T> Get() { return Dequeue(); }


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
                if (IsEmpty()) {
                    throw std::out_of_range("CircularQueue: Queue is empty, cannot peek");
                }
                return data_vector_[head_];
            }


            // --- Status Functions ---
            /**
             * @brief 检查队列是否为空。
             *
             * @return 如果队列中当前没有元素，返回 true；否则返回 false。
             */
            // Check if the queue is empty
            bool IsEmpty() const {
                std::lock_guard<std::mutex> lock(mutex_); // Thread safe empty
                return current_size_ == 0; // Or head_ == tail_ and current_size_ == 0;
            }

            /**
             * @brief 检查队列是否已满。
             *
             * @return 如果队列中当前存储的元素数量等于最大可用容量，返回 true；否则返回 false。
             */
            // Check if the queue is full
            bool IsFull() const {
                std::lock_guard<std::mutex> lock(mutex_); // Thread safe full
                // Standard check for circular queue full: tail is one position behind head (modulo capacity_)
                return (tail_ + 1) % capacity_ == head_;
                // Alternative using current_size_: return current_size_ == capacity_ - 1;
            }

            /**
             * @brief 获取队列中当前存储的元素数量。
             *
             * @return 队列中当前存储的元素数量。
             */
            // Get the number of elements currently in the queue
            size_t Size() const {
                std::lock_guard<std::mutex> lock(mutex_); // Thread safe size
                return current_size_;
            }

            /**
             * @brief 获取队列的最大可用容量。
             *
             * @return 队列的最大可用容量（元素数量）。
             */
            // Get the maximum usable capacity of the queue
            size_t Capacity() const {
                return capacity_ - 1; // Return actual usable capacity
            }
        };

    } // namespace Memory
} // namespace LIB_LSX

#endif // LIB_LSX_MEMORY_CIRCULAR_QUEUE_H
