/**
 * @file FixedSizePipe.h
 * @brief 固定大小内存块管道类
 * @details 定义了 LSX_LIB::Memory 命名空间下的 FixedSizePipe 类，
 * 用于实现一个线程安全的、传输固定大小内存块的管道（通常用作消息队列）。
 * 该管道基于一个内部的 std::vector 缓冲区，并使用头部和尾部索引来管理块的读写。
 * 提供非阻塞和阻塞（带超时）的数据写入（Write）和读取（Read）操作，
 * 并使用 std::mutex 和 std::condition_variable 来保证在多线程环境下的线程安全访问和同步。
 * @author 连思鑫（liansixin）
 * @date 2025-5-7
 * @version 1.0
 *
 * ### 核心功能
 * - **固定大小块传输**: 管道传输的是固定大小的内存块，块大小在构造时指定。
 * - **循环缓冲区**: 利用底层线性缓冲区模拟循环管道的行为，高效利用内存。
 * - **非阻塞操作**: 提供 `Write` 和 `Read` 方法，在管道满或空时立即返回。
 * - **阻塞操作**: 提供 `WriteBlocking` 和 `ReadBlocking` 方法，支持无限等待、非阻塞或带超时等待。
 * - **线程安全**: 使用内部互斥锁 (`std::mutex`) 保护对管道状态和缓冲区的并发访问，使用条件变量 (`std::condition_variable`) 实现阻塞操作的线程同步。
 * - **状态查询**: 提供 `IsEmpty`, `IsFull`, `Size`, `BlockSize`, `BlockCount`, `TotalSize` 方法查询管道状态和属性。
 * - **Peek 操作**: 支持查看管道头部的块而不将其移除。
 * - **资源管理**: RAII 模式，`std::vector` 自动管理底层内存。
 *
 * ### 使用示例
 *
 * @code
 * #include "FixedSizePipe.h"
 * #include <iostream>
 * #include <vector>
 * #include <thread>
 * #include <chrono>
 * #include <numeric> // For std::iota
 * #include <optional>
 *
 * // 创建一个块大小为 16 字节，总共 10 个块的管道
 * LSX_LIB::Memory::FixedSizePipe pipe(16, 10);
 *
 * // 写入者线程
 * void writer(int id) {
 * std::vector<uint8_t> data(pipe.BlockSize());
 * for (int i = 0; i < 15; ++i) {
 * // 填充数据，例如块编号
 * std::iota(data.begin(), data.end(), static_cast<uint8_t>(i + id * 100));
 * std::cout << "[Writer " << id << "] Attempting to write block " << i << "..." << std::endl;
 * // 使用阻塞 Write，等待管道有空间
 * if (pipe.WriteBlocking(data)) {
 * std::cout << "[Writer " << id << "] Successfully wrote block " << i << ". Pipe size: " << pipe.Size() << std::endl;
 * } else {
 * std::cerr << "[Writer " << id << "] Failed to write block " << i << " (timeout or error)." << std::endl;
 * }
 * std::this_thread::sleep_for(std::chrono::milliseconds(30)); // 短暂休息
 * }
 * }
 *
 * // 读取者线程
 * void reader(int id) {
 * std::vector<uint8_t> buffer(pipe.BlockSize());
 * for (int i = 0; i < 15; ++i) {
 * std::cout << "[Reader " << id << "] Attempting to read block " << i << "..." << std::endl;
 * // 使用阻塞 Read，等待管道有数据
 * if (pipe.ReadBlocking(buffer)) {
 * std::cout << "[Reader " << id << "] Successfully read block " << i << ". Pipe size: " << pipe.Size() << ". Data: ";
 * for (uint8_t byte : buffer) {
 * std::cout << static_cast<int>(byte) << " ";
 * }
 * std::cout << std::endl;
 * } else {
 * std::cerr << "[Reader " << id << "] Failed to read block " << i << " (timeout or error)." << std::endl;
 * }
 * std::this_thread::sleep_for(std::chrono::milliseconds(100)); // 模拟处理时间
 * }
 * }
 *
 * int main() {
 * std::cout << "Pipe created with block size " << pipe.BlockSize()
 * << ", block count " << pipe.BlockCount()
 * << ", total size " << pipe.TotalSize() << std::endl;
 *
 * std::thread w1(writer, 1);
 * std::thread r1(reader, 1);
 *
 * w1.join();
 * r1.join();
 *
 * std::cout << "All threads finished." << std::endl;
 * std::cout << "Final pipe size: " << pipe.Size() << std::endl;
 *
 * // 示例：非阻塞操作
 * pipe.Clear();
 * std::vector<uint8_t> test_data(pipe.BlockSize(), 0xEE);
 * if (pipe.Write(test_data)) {
 * std::cout << "Write one block non-blocking success." << std::endl;
 * }
 * std::vector<uint8_t> read_data(pipe.BlockSize());
 * if (pipe.Read(read_data.data(), read_data.size())) {
 * std::cout << "Read one block non-blocking success." << std::endl;
 * }
 *
 * std::cout << "主线程退出。" << std::endl;
 * return 0;
 * }
 * @endcode
 *
 * ### 注意事项
 * - **固定大小**: 管道只能传输固定大小的内存块。尝试 Write 写入不同大小的数据会失败。
 * - **内存复制**: `Write`, `Read`, `Peek` 方法都涉及数据的复制。对于需要避免复制的场景，可能需要修改接口以返回指向内部缓冲区的指针（但需谨慎处理生命周期和线程安全）。
 * - **线程安全**: 所有公共方法都通过互斥锁保护，支持多线程访问。阻塞方法使用条件变量进行同步。
 * - **阻塞超时**: 阻塞方法的超时参数以毫秒为单位。超时为 -1 表示无限等待，0 表示非阻塞。
 * - **异常处理**: 构造函数可能因内存分配失败抛出 `std::bad_alloc`。公共方法在参数无效或操作失败时通常返回 false 或空 optional，而不是抛出异常。
 * - **拷贝/移动**: 类禁用了拷贝构造和赋值，因为直接拷贝一个包含线程同步原语（mutex, condition_variable）和状态（head, tail, size）的管道通常是不安全或没有意义的。如果需要传递管道对象，应考虑使用智能指针（如 `std::shared_ptr`）。移动语义未默认实现，如果需要，需要手动实现以安全转移资源和状态。
 */

#ifndef LSX_LIB_MEMORY_FIXED_SIZE_PIPE_H
#define LSX_LIB_MEMORY_FIXED_SIZE_PIPE_H
#pragma once
// 包含全局错误锁头文件
#include "GlobalErrorMutex.h"
// 包含 LSX_LIB::LockManager::LockGuard 头文件
#include "LockGuard.h"
#include <vector> // For std::vector
#include <cstdint> // For uint8_t
#include <optional> // For std::optional (C++17)
#include <stdexcept> // For exceptions
#include <mutex>   // For thread safety (std::mutex, std::lock_guard, std::unique_lock)
#include <condition_variable> // For blocking operations (std::condition_variable)
#include <cstring> // For memcpy
#include <chrono> // For std::chrono::milliseconds, std::chrono::duration_cast
#include <algorithm> // For std::min
// #include <iostream> // For example output - prefer logging


/**
 * @brief LSX 库的根命名空间。
 */
namespace LSX_LIB {
    /**
     * @brief 内存管理相关的命名空间。
     * 包含内存缓冲区和相关工具。
     */
    namespace Memory {

        /**
         * @brief 固定大小内存块管道类。
         * 实现一个线程安全的、用于传输固定大小内存块的管道（消息队列）。
         * 支持非阻塞和阻塞（带超时）的写入和读取操作。
         */
        // 9. FixedSizePipe Module (固定大小块管道模块)
        // 实现传输固定大小内存块的管道 (通常用于消息队列)
        // 线程安全：使用 std::mutex 和 std::condition_variable
        class FixedSizePipe {
        private:
            /**
             * @brief 存储管道数据的底层内存缓冲区。
             * 大小为 block_size_ * block_count_。
             */
            std::vector<uint8_t> buffer_; // Underlying memory buffer
            /**
             * @brief 每个内存块的大小（字节）。
             * 在构造时确定。
             */
            size_t block_size_;
            /**
             * @brief 管道可以存储的最大块数量。
             * 在构造时确定。
             */
            size_t block_count_; // Total number of blocks
            /**
             * @brief 管道头部索引。
             * 指向下一个要读取的块的起始位置（以块为单位的索引）。
             */
            size_t head_ = 0; // Index of the next block to read
            /**
             * @brief 管道尾部索引。
             * 指向下一个新块将要写入的位置（以块为单位的索引）。
             */
            size_t tail_ = 0; // Index where the next block will be written
            /**
             * @brief 管道中当前存储的块数量。
             */
            size_t current_size_ = 0; // Number of blocks currently in the pipe
            /**
             * @brief 互斥锁。
             * 用于保护管道的状态变量 (head_, tail_, current_size_) 和底层缓冲区 (buffer_) 的并发访问。
             */
            mutable std::mutex mutex_; // Thread safety mutex
            /**
             * @brief 条件变量，用于阻塞读取操作。
             * 当管道为空时，读取线程在此等待，直到有新数据写入。
             */
            std::condition_variable cv_read_; // For blocking reads
            /**
             * @brief 条件变量，用于阻塞写入操作。
             * 当管道满时，写入线程在此等待，直到有空间可用。
             */
            std::condition_variable cv_write_; // For blocking writes

            /**
             * @brief 辅助函数，获取指定块索引在底层缓冲区中的内存地址。
             * 此函数假定调用者已持有互斥锁，以保证线程安全访问。
             *
             * @param index 块的索引（0 到 block_count_ - 1）。
             * @return 指向该块起始位置的 uint8_t 指针。
             */
            // Helper to get the memory address for a block index (assumes lock is held)
            uint8_t* get_block_address(size_t index);
            /**
             * @brief 辅助函数，获取指定块索引在底层缓冲区中的常量内存地址。
             * 此函数假定调用者已持有互斥锁，以保证线程安全访问。
             *
             * @param index 块的索引（0 到 block_count_ - 1）。
             * @return 指向该块起始位置的 const uint8_t 指针。
             */
            const uint8_t* get_block_address(size_t index) const;


        public:
            /**
             * @brief 构造函数。
             * 初始化固定大小管道，分配底层缓冲区。
             *
             * @param block_size 每个内存块的大小（字节）。必须大于 0。
             * @param block_count 管道可以存储的最大块数量。必须大于 0。
             * @throws std::invalid_argument 如果 block_size 或 block_count 为 0。
             * @throws std::bad_alloc 如果底层缓冲区内存分配失败。
             */
            // Constructor: block_size is size of each block, block_count is max number of blocks
            FixedSizePipe(size_t block_size, size_t block_count);
            /**
             * @brief 析构函数。
             * 默认析构函数，由 std::vector 自动管理底层内存释放。
             */
            ~FixedSizePipe() = default;

            // Prevent copying and assignment
            /**
             * @brief 禁用拷贝构造函数。
             * 固定大小管道包含同步原语和状态，拷贝不安全。
             */
            FixedSizePipe(const FixedSizePipe&) = delete;
            /**
             * @brief 禁用拷贝赋值运算符。
             * 固定大小管道包含同步原语和状态，拷贝不安全。
             */
            FixedSizePipe& operator=(const FixedSizePipe&) = delete;
            // Consider adding move constructor/assignment

            // --- Management Functions ---
            /**
             * @brief 清空管道。
             * 重置管道的头部、尾部和当前大小，但不释放底层内存。
             * 使管道回到初始的空状态。
             */
            // Clear the pipe (resets head/tail/size)
            void Clear();

            // --- Data Access Functions (Non-blocking) ---
            /**
             * @brief 将一个固定大小的块写入管道尾部 (非阻塞)。
             * 将指定数据复制到管道尾部的下一个可用位置。
             * 如果管道已满或数据大小与块大小不匹配，立即返回 false。
             *
             * @param data 指向要写入数据的缓冲区。
             * @param data_size 要写入数据的字节数。必须等于 BlockSize()。
             * @return 如果成功写入块，返回 true；如果管道已满或 data_size 不匹配，返回 false。
             */
            // Write a fixed-size block to the pipe
            // data must point to size_t data_size bytes. data_size must equal BlockSize().
            // Returns true on success, false if pipe is full or data_size mismatch.
            bool Write(const uint8_t* data, size_t data_size);
            /**
             * @brief 将 std::vector 中的数据作为一个块写入管道尾部 (非阻塞)。
             * 便利方法，将 std::vector 中的数据复制到管道尾部的下一个可用位置。
             * 如果管道已满或 vector 大小与块大小不匹配，立即返回 false。
             *
             * @param data 包含要写入数据的 std::vector。其大小必须等于 BlockSize()。
             * @return 如果成功写入块，返回 true；如果管道已满或 data.size() 不匹配，返回 false。
             */
            bool Write(const std::vector<uint8_t>& data); // Convenience method

            /**
             * @brief 从管道头部读取一个固定大小的块 (非阻塞)。
             * 将管道头部的数据复制到提供的缓冲区，并从管道中移除该块。
             * 如果管道为空或提供的缓冲区大小不足，立即返回 false。
             *
             * @param buffer 指向用于存储读取数据的缓冲区。其大小必须至少为 BlockSize()。
             * @param buffer_size 提供的缓冲区大小。必须至少为 BlockSize()。
             * @return 如果成功读取块，返回 true；如果管道为空或 buffer_size 太小，返回 false。
             */
            // Read a fixed-size block from the pipe
            // Copies data into the provided buffer. buffer_size must be at least BlockSize().
            // Returns true on success, false if pipe is empty or buffer_size too small.
            bool Read(uint8_t* buffer, size_t buffer_size);
            /**
             * @brief 从管道头部读取一个固定大小的块，并返回其拷贝 (非阻塞)。
             * 便利方法，将管道头部的数据复制到一个新的 std::vector 中，并从管道中移除该块。
             *
             * @return 一个包含读取数据的 std::optional<std::vector<uint8_t>>。如果管道为空，返回 std::nullopt。
             */
            std::optional<std::vector<uint8_t>> Read(); // Convenience method returning a copy


            /**
             * @brief 查看管道头部的块，但不移除 (非阻塞)。
             * 将管道头部的数据复制到提供的缓冲区。管道保持不变。
             * 如果管道为空或提供的缓冲区大小不足，立即返回 false。
             *
             * @param buffer 指向用于存储查看数据的缓冲区。其大小必须至少为 BlockSize()。
             * @param buffer_size 提供的缓冲区大小。必须至少为 BlockSize()。
             * @return 如果成功查看块，返回 true；如果管道为空或 buffer_size 太小，返回 false。
             */
            // Peek at the block at the front of the pipe without removing it
            // Copies data into the provided buffer. buffer_size must be at least BlockSize().
            // Returns true on success, false if pipe is empty or buffer_size too small.
            bool Peek(uint8_t* buffer, size_t buffer_size) const;
            /**
             * @brief 查看管道头部的块，并返回其拷贝，但不移除 (非阻塞)。
             * 便利方法，将管道头部的数据复制到一个新的 std::vector 中。管道保持不变。
             *
             * @return 一个包含查看数据的 std::optional<std::vector<uint8_t>>。如果管道为空，返回 std::nullopt。
             */
            std::optional<std::vector<uint8_t>> Peek() const; // Convenience method returning a copy


            // --- Data Access Functions (Blocking) ---
            /**
             * @brief 从管道头部读取一个固定大小的块 (阻塞)。
             * 将管道头部的数据复制到提供的缓冲区，并从管道中移除该块。
             * 如果管道为空，线程将等待直到有数据可用，或直到超时发生。
             *
             * @param buffer 指向用于存储读取数据的缓冲区。其大小必须至少为 BlockSize()。
             * @param buffer_size 提供的缓冲区大小。必须至少为 BlockSize()。
             * @param timeout_ms 等待超时时间，单位为毫秒。
             * - < 0: 无限等待。
             * - == 0: 非阻塞（行为同非阻塞 Read）。
             * - > 0: 最多等待指定的毫秒数。
             * @return 如果成功读取块，返回 true；如果超时、管道为空（非阻塞模式）或 buffer_size 太小，返回 false。
             */
            // Read a fixed-size block from the pipe, waits if empty.
            // timeout_ms < 0: infinite wait
            // timeout_ms == 0: non-blocking (same as non-blocking Read)
            // timeout_ms > 0: timed wait
            // Returns true on success, false on timeout or error.
            bool ReadBlocking(uint8_t* buffer, size_t buffer_size, long timeout_ms = -1);
            /**
             * @brief 从管道头部读取一个固定大小的块，并返回其拷贝 (阻塞)。
             * 便利方法，将管道头部的数据复制到一个新的 std::vector 中，并从管道中移除该块。
             * 如果管道为空，线程将等待直到有数据可用，或直到超时发生。
             *
             * @param timeout_ms 等待超时时间，单位为毫秒。参见 ReadBlocking(uint8_t*, size_t, long) 的说明。
             * @return 一个包含读取数据的 std::optional<std::vector<uint8_t>>。如果管道为空（且超时/非阻塞返回）或超时发生，返回 std::nullopt。
             */
            std::optional<std::vector<uint8_t>> ReadBlocking(long timeout_ms = -1); // Convenience returning a copy


            /**
             * @brief 将一个固定大小的块写入管道尾部 (阻塞)。
             * 将指定数据复制到管道尾部的下一个可用位置。
             * 如果管道已满，线程将等待直到有空间可用，或直到超时发生。
             *
             * @param data 指向要写入数据的缓冲区。
             * @param data_size 要写入数据的字节数。必须等于 BlockSize()。
             * @param timeout_ms 等待超时时间，单位为毫秒。
             * - < 0: 无限等待。
             * - == 0: 非阻塞（行为同非阻塞 Write）。
             * - > 0: 最多等待指定的毫秒数。
             * @return 如果成功写入块，返回 true；如果超时、管道已满（非阻塞模式）或 data_size 不匹配，返回 false。
             */
            // Returns true on success, false on timeout or error.
            bool WriteBlocking(const uint8_t* data, size_t data_size, long timeout_ms = -1);
            /**
             * @brief 将 std::vector 中的数据作为一个块写入管道尾部 (阻塞)。
             * 便利方法，将 std::vector 中的数据复制到管道尾部的下一个可用位置。
             * 如果管道已满，线程将等待直到有空间可用，或直到超时发生。
             *
             * @param data 包含要写入数据的 std::vector。其大小必须等于 BlockSize()。
             * @param timeout_ms 等待超时时间，单位为毫秒。参见 WriteBlocking(const uint8_t*, size_t, long) 的说明。
             * @return 如果成功写入块，返回 true；如果超时、管道已满（非阻塞模式）或 data.size() 不匹配，返回 false。
             */
            bool WriteBlocking(const std::vector<uint8_t>& data, long timeout_ms = -1); // Convenience

            // --- Status Functions ---
            /**
             * @brief 检查管道是否为空。
             *
             * @return 如果管道中当前没有块，返回 true；否则返回 false。
             */
            // Check if the pipe is empty
            bool IsEmpty() const;

            /**
             * @brief 检查管道是否已满。
             *
             * @return 如果管道中当前存储的块数量等于最大块数量，返回 true；否则返回 false。
             */
            // Check if the pipe is full
            bool IsFull() const;

            /**
             * @brief 获取管道中当前存储的块数量。
             *
             * @return 管道中当前存储的块数量。
             */
            // Get the number of blocks currently in the pipe
            size_t Size() const;

            /**
             * @brief 获取每个内存块的大小。
             *
             * @return 每个内存块的大小（字节）。
             */
            // Get the size of each block
            size_t BlockSize() const { return block_size_; }

            /**
             * @brief 获取管道可以存储的最大块数量。
             *
             * @return 管道的最大块数量。
             */
            // Get the maximum number of blocks the pipe can hold
            size_t BlockCount() const { return block_count_; }

            /**
             * @brief 获取为存储块分配的总内存大小。
             *
             * @return 总内存大小（字节），等于 BlockSize() * BlockCount()。
             */
            // Get the total memory size allocated for blocks (BlockSize * BlockCount)
            size_t TotalSize() const { return block_size_ * block_count_; }
        };

    } // namespace Memory
} // namespace LSX_LIB

#endif // LSX_LIB_MEMORY_FIXED_SIZE_PIPE_H
