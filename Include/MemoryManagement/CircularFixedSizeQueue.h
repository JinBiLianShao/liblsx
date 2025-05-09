/**
 * @file CircularFixedSizeQueue.h
 * @brief 循环固定大小内存块队列类
 * @details 定义了 LIB_LSX::Memory 命名空间下的 CircularFixedSizeQueue 类，
 * 用于实现一个存储固定大小内存块的循环队列。
 * 该队列基于一个内部的 std::vector 缓冲区，并使用头部和尾部索引来管理块的存取。
 * 提供非阻塞和阻塞（带超时）的数据放入（Put）和取出（Get）操作，
 * 并使用 std::mutex 和 std::condition_variable 来保证在多线程环境下的线程安全访问和同步。
 * @author 连思鑫（liansixin）
 * @date 2025-5-7
 * @version 1.0
 *
 * ### 核心功能
 * - **固定大小块存储**: 队列存储的是固定大小的内存块，块大小在构造时指定。
 * - **循环队列**: 利用底层线性缓冲区模拟循环队列的行为，高效利用内存。
 * - **非阻塞操作**: 提供 `Put` 和 `Get` 方法，在队列满或空时立即返回。
 * - **阻塞操作**: 提供 `PutBlocking` 和 `GetBlocking` 方法，支持无限等待、非阻塞或带超时等待。
 * - **线程安全**: 使用内部互斥锁 (`std::mutex`) 保护对队列状态和缓冲区的并发访问，使用条件变量 (`std::condition_variable`) 实现阻塞操作的线程同步。
 * - **状态查询**: 提供 `IsEmpty`, `IsFull`, `Size`, `BlockSize`, `BlockCount`, `TotalSize` 方法查询队列状态和属性。
 * - **Peek 操作**: 支持查看队列头部的块而不将其移除。
 * - **资源管理**: RAII 模式，`std::vector` 自动管理底层内存。
 *
 * ### 使用示例
 *
 * @code
 * #include "CircularFixedSizeQueue.h"
 * #include <iostream>
 * #include <vector>
 * #include <thread>
 * #include <chrono>
 * #include <numeric> // For std::iota
 *
 * // 创建一个块大小为 10 字节，总共 5 个块的队列
 * LIB_LSX::Memory::CircularFixedSizeQueue queue(10, 5);
 *
 * // 生产者线程
 * void producer(int id) {
 * std::vector<uint8_t> data(queue.BlockSize());
 * for (int i = 0; i < 10; ++i) {
 * // 填充数据，例如块编号
 * std::iota(data.begin(), data.end(), static_cast<uint8_t>(i + id * 10));
 * std::cout << "[Producer " << id << "] Attempting to put block " << i << "..." << std::endl;
 * // 使用阻塞 Put，等待队列有空间
 * if (queue.PutBlocking(data)) {
 * std::cout << "[Producer " << id << "] Successfully put block " << i << ". Queue size: " << queue.Size() << std::endl;
 * } else {
 * std::cerr << "[Producer " << id << "] Failed to put block " << i << " (timeout or error)." << std::endl;
 * }
 * std::this_thread::sleep_for(std::chrono::milliseconds(50)); // 短暂休息
 * }
 * }
 *
 * // 消费者线程
 * void consumer(int id) {
 * std::vector<uint8_t> buffer(queue.BlockSize());
 * for (int i = 0; i < 10; ++i) {
 * std::cout << "[Consumer " << id << "] Attempting to get block " << i << "..." << std::endl;
 * // 使用阻塞 Get，等待队列有数据
 * if (queue.GetBlocking(buffer)) {
 * std::cout << "[Consumer " << id << "] Successfully got block " << i << ". Queue size: " << queue.Size() << ". Data: ";
 * for (uint8_t byte : buffer) {
 * std::cout << static_cast<int>(byte) << " ";
 * }
 * std::cout << std::endl;
 * } else {
 * std::cerr << "[Consumer " << id << "] Failed to get block " << i << " (timeout or error)." << std::endl;
 * }
 * std::this_thread::sleep_for(std::chrono::milliseconds(150)); // 模拟处理时间
 * }
 * }
 *
 * int main() {
 * std::cout << "Queue created with block size " << queue.BlockSize()
 * << ", block count " << queue.BlockCount()
 * << ", total size " << queue.TotalSize() << std::endl;
 *
 * std::thread p1(producer, 1);
 * std::thread c1(consumer, 1);
 *
 * p1.join();
 * c1.join();
 *
 * std::cout << "All threads finished." << std::endl;
 * std::cout << "Final queue size: " << queue.Size() << std::endl;
 *
 * // 示例：非阻塞操作
 * queue.Clear();
 * std::vector<uint8_t> test_data(queue.BlockSize(), 0xFF);
 * if (queue.Put(test_data)) {
 * std::cout << "Put one block non-blocking success." << std::endl;
 * }
 * std::vector<uint8_t> read_data(queue.BlockSize());
 * if (queue.Get(read_data.data(), read_data.size())) {
 * std::cout << "Get one block non-blocking success." << std::endl;
 * }
 *
 * std::cout << "主线程退出。" << std::endl;
 * return 0;
 * }
 * @endcode
 *
 * ### 注意事项
 * - **固定大小**: 队列只能存储固定大小的内存块。尝试 Put 放入不同大小的数据会失败。
 * - **内存复制**: `Put`, `Get`, `Peek` 方法都涉及数据的复制。对于需要避免复制的场景，可能需要修改接口以返回指向内部缓冲区的指针（但需谨慎处理生命周期和线程安全）。
 * - **线程安全**: 所有公共方法都通过互斥锁保护，支持多线程访问。阻塞方法使用条件变量进行同步。
 * - **阻塞超时**: 阻塞方法的超时参数以毫秒为单位。超时为 -1 表示无限等待，0 表示非阻塞。
 * - **异常处理**: 构造函数可能因内存分配失败抛出 `std::bad_alloc`。公共方法在参数无效或操作失败时通常返回 false 或空 optional，而不是抛出异常。
 * - **拷贝/移动**: 类禁用了拷贝构造和赋值，因为直接拷贝一个包含线程同步原语（mutex, condition_variable）和状态（head, tail, size）的队列通常是不安全或没有意义的。如果需要传递队列对象，应考虑使用智能指针（如 `std::shared_ptr`）。移动语义未默认实现，如果需要，需要手动实现以安全转移资源和状态。
 */

#ifndef LIB_LSX_MEMORY_CIRCULAR_FIXED_SIZE_QUEUE_H
#define LIB_LSX_MEMORY_CIRCULAR_FIXED_SIZE_QUEUE_H
#pragma once
// 包含全局错误锁头文件
#include "GlobalErrorMutex.h"
// 包含 LIBLSX::LockManager::LockGuard 头文件
#include "LockGuard.h"
#include <vector> // For std::vector
#include <cstdint> // For uint8_t
#include <optional> // For std::optional (C++17)
#include <stdexcept> // For exceptions
#include <mutex>    // For thread safety (std::mutex, std::lock_guard, std::unique_lock)
#include <condition_variable> // For potential blocking operations (std::condition_variable)
#include <cstring> // For memcpy
#include <chrono> // For std::chrono::milliseconds, std::chrono::duration_cast
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
         * @brief 循环固定大小内存块队列类。
         * 实现一个线程安全的、存储固定大小内存块的循环队列。
         * 支持非阻塞和阻塞（带超时）的 Put 和 Get 操作。
         */
        // 8. CircularFixedSizeQueue Module (循环固定大小块队列模块)
        // 实现存储固定大小内存块的循环队列
        // 线程安全：使用 std::mutex 和 std::condition_variable (用于阻塞操作)
        class CircularFixedSizeQueue {
        private:
            /**
             * @brief 存储队列数据的底层内存缓冲区。
             * 大小为 block_size_ * block_count_。
             */
            std::vector<uint8_t> buffer_; // Underlying memory buffer
            /**
             * @brief 每个内存块的大小（字节）。
             * 在构造时确定。
             */
            size_t block_size_;
            /**
             * @brief 队列可以存储的最大块数量。
             * 在构造时确定。
             */
            size_t block_count_; // Total number of blocks
            /**
             * @brief 队列头部索引。
             * 指向队列中最旧的有效块的起始位置（以块为单位的索引）。
             */
            size_t head_ = 0; // Index of the front valid block
            /**
             * @brief 队列尾部索引。
             * 指向下一个新块将要放入的位置（以块为单位的索引）。
             */
            size_t tail_ = 0; // Index where the next block will be put
            /**
             * @brief 队列中当前存储的块数量。
             */
            size_t current_size_ = 0; // Number of blocks currently in the queue
            /**
             * @brief 互斥锁。
             * 用于保护队列的状态变量 (head_, tail_, current_size_) 和底层缓冲区 (buffer_) 的并发访问。
             */
            mutable std::mutex mutex_; // Thread safety mutex
            /**
             * @brief 条件变量，用于阻塞读取操作。
             * 当队列为空时，消费者线程在此等待，直到有新数据放入。
             */
            std::condition_variable cv_read_; // For blocking reads
            /**
             * @brief 条件变量，用于阻塞写入操作。
             * 当队列满时，生产者线程在此等待，直到有空间可用。
             */
            std::condition_variable cv_write_; // For blocking writes


            /**
             * @brief 辅助函数，获取指定块索引在底层缓冲区中的内存地址。
             *
             * @param index 块的索引（0 到 block_count_ - 1）。
             * @return 指向该块起始位置的 uint8_t 指针。
             */
            uint8_t* get_block_address(size_t index);
            /**
             * @brief 辅助函数，获取指定块索引在底层缓冲区中的常量内存地址。
             *
             * @param index 块的索引（0 到 block_count_ - 1）。
             * @return 指向该块起始位置的 const uint8_t 指针。
             */
            const uint8_t* get_block_address(size_t index) const;


        public:
            /**
             * @brief 构造函数。
             * 初始化循环固定大小队列，分配底层缓冲区。
             *
             * @param block_size 每个内存块的大小（字节）。必须大于 0。
             * @param block_count 队列可以存储的最大块数量。必须大于 0。
             * @throws std::invalid_argument 如果 block_size 或 block_count 为 0。
             * @throws std::bad_alloc 如果底层缓冲区内存分配失败。
             */
            // Constructor: block_size is size of each block, block_count is max number of blocks
            CircularFixedSizeQueue(size_t block_size, size_t block_count);
            /**
             * @brief 析构函数。
             * 默认析构函数，由 std::vector 自动管理底层内存释放。
             */
            ~CircularFixedSizeQueue() = default;

            // Prevent copying and assignment
            /**
             * @brief 禁用拷贝构造函数。
             * 循环队列包含同步原语和状态，拷贝不安全。
             */
            CircularFixedSizeQueue(const CircularFixedSizeQueue&) = delete;
            /**
             * @brief 禁用拷贝赋值运算符。
             * 循环队列包含同步原语和状态，拷贝不安全。
             */
            CircularFixedSizeQueue& operator=(const CircularFixedSizeQueue&) = delete;
            // Consider adding move constructor/assignment if state transfer is safe

            // --- Management Functions ---
            /**
             * @brief 清空队列。
             * 重置队列的头部、尾部和当前大小，但不释放底层内存。
             * 使队列回到初始的空状态。
             */
            void Clear();

            // --- Data Access Functions (Non-blocking) ---
            /**
             * @brief 将一个固定大小的块放入队列尾部 (非阻塞)。
             * 将指定数据复制到队列尾部的下一个可用位置。
             * 如果队列已满或数据大小与块大小不匹配，立即返回 false。
             *
             * @param data 指向要放入数据的缓冲区。
             * @param data_size 要放入数据的字节数。必须等于 BlockSize()。
             * @return 如果成功放入块，返回 true；如果队列已满或 data_size 不匹配，返回 false。
             */
            // data must point to size_t data_size bytes. data_size must equal BlockSize().
            // Returns true on success, false if queue is full or data_size mismatch.
            bool Put(const uint8_t* data, size_t data_size);
            /**
             * @brief 将 std::vector 中的数据作为一个块放入队列尾部 (非阻塞)。
             * 便利方法，将 std::vector 中的数据复制到队列尾部的下一个可用位置。
             * 如果队列已满或 vector 大小与块大小不匹配，立即返回 false。
             *
             * @param data 包含要放入数据的 std::vector。其大小必须等于 BlockSize()。
             * @return 如果成功放入块，返回 true；如果队列已满或 data.size() 不匹配，返回 false。
             */
            bool Put(const std::vector<uint8_t>& data); // Convenience method


            /**
             * @brief 从队列头部取出一个固定大小的块 (非阻塞)。
             * 将队列头部的数据复制到提供的缓冲区，并从队列中移除该块。
             * 如果队列为空或提供的缓冲区大小不足，立即返回 false。
             *
             * @param buffer 指向用于存储取出数据的缓冲区。其大小必须至少为 BlockSize()。
             * @param buffer_size 提供的缓冲区大小。必须至少为 BlockSize()。
             * @return 如果成功取出块，返回 true；如果队列为空或 buffer_size 太小，返回 false。
             */
            // Copies data into the provided buffer. buffer_size must be at least BlockSize().
            // Returns true on success, false if queue is empty or buffer_size too small.
            bool Get(uint8_t* buffer, size_t buffer_size);
            /**
             * @brief 从队列头部取出一个固定大小的块，并返回其拷贝 (非阻塞)。
             * 便利方法，将队列头部的数据复制到一个新的 std::vector 中，并从队列中移除该块。
             *
             * @return 一个包含取出数据的 std::optional<std::vector<uint8_t>>。如果队列为空，返回 std::nullopt。
             */
            std::optional<std::vector<uint8_t>> Get(); // Convenience method returning a copy


            /**
             * @brief 查看队列头部的块，但不移除 (非阻塞)。
             * 将队列头部的数据复制到提供的缓冲区。队列保持不变。
             * 如果队列为空或提供的缓冲区大小不足，立即返回 false。
             *
             * @param buffer 指向用于存储查看数据的缓冲区。其大小必须至少为 BlockSize()。
             * @param buffer_size 提供的缓冲区大小。必须至少为 BlockSize()。
             * @return 如果成功查看块，返回 true；如果队列为空或 buffer_size 太小，返回 false。
             */
            // Copies data into the provided buffer. buffer_size must be at least BlockSize().
            // Returns true on success, false if queue is empty or buffer_size too small.
            bool Peek(uint8_t* buffer, size_t buffer_size) const;
            /**
             * @brief 查看队列头部的块，并返回其拷贝，但不移除 (非阻塞)。
             * 便利方法，将队列头部的数据复制到一个新的 std::vector 中。队列保持不变。
             *
             * @return 一个包含查看数据的 std::optional<std::vector<uint8_t>>。如果队列为空，返回 std::nullopt。
             */
            std::optional<std::vector<uint8_t>> Peek() const; // Convenience method returning a copy


            // --- Data Access Functions (Blocking) ---
            /**
             * @brief 从队列头部取出一个固定大小的块 (阻塞)。
             * 将队列头部的数据复制到提供的缓冲区，并从队列中移除该块。
             * 如果队列为空，线程将等待直到有数据可用，或直到超时发生。
             *
             * @param buffer 指向用于存储取出数据的缓冲区。其大小必须至少为 BlockSize()。
             * @param buffer_size 提供的缓冲区大小。必须至少为 BlockSize()。
             * @param timeout_ms 等待超时时间，单位为毫秒。
             * - < 0: 无限等待。
             * - == 0: 非阻塞（行为同非阻塞 Get）。
             * - > 0: 最多等待指定的毫秒数。
             * @return 如果成功取出块，返回 true；如果超时、队列为空（非阻塞模式）或 buffer_size 太小，返回 false。
             */
            // Get a fixed-size block from the queue, waits if empty.
            // timeout_ms < 0: infinite wait
            // timeout_ms == 0: non-blocking (same as non-blocking Get)
            // timeout_ms > 0: timed wait
            // Returns true on success, false on timeout or error.
            bool GetBlocking(uint8_t* buffer, size_t buffer_size, long timeout_ms = -1);
            /**
             * @brief 从队列头部取出一个固定大小的块，并返回其拷贝 (阻塞)。
             * 便利方法，将队列头部的数据复制到一个新的 std::vector 中，并从队列中移除该块。
             * 如果队列为空，线程将等待直到有数据可用，或直到超时发生。
             *
             * @param timeout_ms 等待超时时间，单位为毫秒。参见 GetBlocking(uint8_t*, size_t, long) 的说明。
             * @return 一个包含取出数据的 std::optional<std::vector<uint8_t>>。如果队列为空（且超时/非阻塞返回）或超时发生，返回 std::nullopt。
             */
            std::optional<std::vector<uint8_t>> GetBlocking(long timeout_ms = -1); // Convenience returning a copy


            /**
             * @brief 将一个固定大小的块放入队列尾部 (阻塞)。
             * 将指定数据复制到队列尾部的下一个可用位置。
             * 如果队列已满，线程将等待直到有空间可用，或直到超时发生。
             *
             * @param data 指向要放入数据的缓冲区。
             * @param data_size 要放入数据的字节数。必须等于 BlockSize()。
             * @param timeout_ms 等待超时时间，单位为毫秒。
             * - < 0: 无限等待。
             * - == 0: 非阻塞（行为同非阻塞 Put）。
             * - > 0: 最多等待指定的毫秒数。
             * @return 如果成功放入块，返回 true；如果超时、队列已满（非阻塞模式）或 data_size 不匹配，返回 false。
             */
            // Returns true on success, false on timeout or error.
            bool PutBlocking(const uint8_t* data, size_t data_size, long timeout_ms = -1);
            /**
             * @brief 将 std::vector 中的数据作为一个块放入队列尾部 (阻塞)。
             * 便利方法，将 std::vector 中的数据复制到队列尾部的下一个可用位置。
             * 如果队列已满，线程将等待直到有空间可用，或直到超时发生。
             *
             * @param data 包含要放入数据的 std::vector。其大小必须等于 BlockSize()。
             * @param timeout_ms 等待超时时间，单位为毫秒。参见 PutBlocking(const uint8_t*, size_t, long) 的说明。
             * @return 如果成功放入块，返回 true；如果超时、队列已满（非阻塞模式）或 data.size() 不匹配，返回 false。
             */
            bool PutBlocking(const std::vector<uint8_t>& data, long timeout_ms = -1); // Convenience

            // --- Status Functions ---
            /**
             * @brief 检查队列是否为空。
             *
             * @return 如果队列中当前没有块，返回 true；否则返回 false。
             */
            bool IsEmpty() const;

            /**
             * @brief 检查队列是否已满。
             *
             * @return 如果队列中当前存储的块数量等于最大块数量，返回 true；否则返回 false。
             */
            bool IsFull() const;

            /**
             * @brief 获取队列中当前存储的块数量。
             *
             * @return 队列中当前存储的块数量。
             */
            size_t Size() const;

            /**
             * @brief 获取每个内存块的大小。
             *
             * @return 每个内存块的大小（字节）。
             */
            size_t BlockSize() const { return block_size_; }

            /**
             * @brief 获取队列可以存储的最大块数量。
             *
             * @return 队列的最大块数量。
             */
            size_t BlockCount() const { return block_count_; }

            /**
             * @brief 获取为存储块分配的总内存大小。
             *
             * @return 总内存大小（字节），等于 BlockSize() * BlockCount()。
             */
            size_t TotalSize() const { return block_size_ * block_count_; }
        };

    } // namespace Memory
} // namespace LIB_LSX

#endif // LIB_LSX_MEMORY_CIRCULAR_FIXED_SIZE_QUEUE_H
