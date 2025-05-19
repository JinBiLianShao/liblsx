/**
 * @file Buffer.h
 * @brief 通用内存缓冲区类
 * @details 定义了 LIB_LSX::Memory 命名空间下的 Buffer 类，
 * 用于实现一个通用的、基于 std::vector<uint8_t> 的内存缓冲区。
 * 提供缓冲区的大小管理、数据填充、以及在指定偏移量进行读写操作的功能。
 * 类内部使用 std::mutex 来保证在多线程环境下的线程安全访问。
 * 支持移动语义，允许拷贝和赋值（std::vector 会处理深拷贝）。
 * @author 连思鑫（liansixin）
 * @date 2025-5-7
 * @version 1.0
 *
 * ### 核心功能
 * - **动态大小**: 基于 `std::vector` 实现，支持动态调整缓冲区大小 (`Resize`, `Clear`).
 * - **数据填充**: `Fill` 方法用于将缓冲区填充为特定字节值。
 * - **偏移量读写**: 提供 `WriteAt` 和 `ReadAt` 方法，允许在缓冲区的任意有效位置进行数据读写。
 * - **线程安全**: 使用内部互斥锁 (`std::mutex`) 保护对缓冲区数据的并发访问。
 * - **状态查询**: 提供 `GetSize`, `IsEmpty`, `Capacity` 方法查询缓冲区状态。
 * - **数据指针访问**: `GetData` 方法提供对底层数据指针的访问。
 * - **资源管理**: RAII 模式，`std::vector` 自动管理内存。
 *
 * ### 使用示例
 *
 * @code
 * #include "Buffer.h"
 * #include <iostream>
 * #include <vector>
 * #include <thread>
 * #include <chrono>
 *
 * // 示例：多线程访问 Buffer
 * LIB_LSX::Memory::Buffer shared_buffer(100); // 创建一个大小为 100 的缓冲区
 *
 * void writer_thread(int id) {
 * const uint8_t data_to_write[] = { static_cast<uint8_t>('A' + id), static_cast<uint8_t>('a' + id) };
 * size_t offset = (id * 10) % 90; // 错开写入位置
 * std::cout << "Writer " << id << " attempting to write at offset " << offset << std::endl;
 * size_t written = shared_buffer.WriteAt(offset, data_to_write, sizeof(data_to_write));
 * if (written > 0) {
 * std::cout << "Writer " << id << " wrote " << written << " bytes at offset " << offset << std::endl;
 * } else {
 * std::cerr << "Writer " << id << " failed to write." << std::endl;
 * }
 * }
 *
 * void reader_thread(int id) {
 * std::vector<uint8_t> read_buffer(5);
 * size_t offset = (id * 10 + 1) % 90; // 错开读取位置
 * std::cout << "Reader " << id << " attempting to read 5 bytes at offset " << offset << std::endl;
 * size_t read_bytes = shared_buffer.ReadAt(offset, read_buffer.data(), read_buffer.size());
 * if (read_bytes > 0) {
 * std::cout << "Reader " << id << " read " << read_bytes << " bytes at offset " << offset << ": ";
 * for (size_t i = 0; i < read_bytes; ++i) {
 * std::cout << static_cast<char>(read_buffer[i]);
 * }
 * std::cout << std::endl;
 * } else if (read_bytes == 0) {
 * std::cout << "Reader " << id << " read 0 bytes (offset out of bounds or size 0)." << std::endl;
 * } else { // read_bytes < 0 indicates error in some potential future implementation
 * std::cerr << "Reader " << id << " encountered an error during read." << std::endl;
 * }
 * }
 *
 * int main() {
 * // 填充初始数据
 * shared_buffer.Fill(0xAA);
 * std::cout << "Buffer size: " << shared_buffer.GetSize() << std::endl;
 *
 * // 启动多个读写线程
 * std::vector<std::thread> threads;
 * for (int i = 0; i < 5; ++i) {
 * threads.emplace_back(writer_thread, i);
 * threads.emplace_back(reader_thread, i);
 * }
 *
 * // 等待所有线程完成
 * for (auto& t : threads) {
 * t.join();
 * }
 *
 * // 调整缓冲区大小
 * shared_buffer.Resize(50);
 * std::cout << "Buffer resized to: " << shared_buffer.GetSize() << std::endl;
 *
 * // 清空缓冲区
 * shared_buffer.Clear();
 * std::cout << "Buffer cleared. IsEmpty: " << shared_buffer.IsEmpty() << std::endl;
 *
 * std::cout << "主线程退出。" << std::endl;
 * return 0;
 * }
 * @endcode
 *
 * ### 注意事项
 * - **线程安全**: 类内部的互斥锁保护了对 `data_vector_` 的直接访问和修改操作。但是，如果用户获取了底层数据指针 (`GetData()`) 并在锁释放后通过指针进行修改，则需要用户自行保证线程安全。
 * - **异常处理**: 底层 `std::vector` 操作（如 `resize`）可能抛出 `std::bad_alloc` 异常。`WriteAt`/`ReadAt` 在偏移量或大小超出缓冲区范围时返回 0，而不是抛出异常。
 * - **性能**: 每次读写操作都会获取和释放互斥锁，在高并发且频繁小数据读写的场景下可能会引入锁竞争开销。
 * - **数据复制**: `WriteAt` 和 `ReadAt` 方法涉及数据的复制。对于大量数据的传输，可能需要考虑更高效的方式（例如，直接操作指针，但需用户自行管理同步）。
 */

#ifndef LIB_LSX_MEMORY_BUFFER_H
#define LIB_LSX_MEMORY_BUFFER_H
#pragma once
// 包含全局错误锁头文件
#include "GlobalErrorMutex.h"
// 包含 LIBLSX::LockManager::LockGuard 头文件
#include "LockGuard.h"
#include <vector> // For std::vector
#include <cstdint> // For uint8_t
#include <stdexcept> // For exceptions like bad_alloc, out_of_range
#include <mutex>    // For thread safety (std::mutex, std::lock_guard)
#include <algorithm> // For std::min, std::fill
// #include <iostream> // For example output - prefer logging

/**
 * @brief LSX 库的根命名空间。
 */
    namespace LIB_LSX::Memory {

        /**
         * @brief 通用内存缓冲区类。
         * 基于 std::vector<uint8_t> 实现的动态大小内存缓冲区。
         * 提供缓冲区管理、数据读写和线程安全访问功能。
         */
        // 6. Buffer Module (缓冲区模块)
        // 用于实现通用的内存缓冲区 (非模板类, 存储字节)
        // 接口优化：增加Clear, Fill, WriteAt/ReadAt
        // 线程安全：使用 std::mutex
        class Buffer {
        private:
            /**
             * @brief 存储缓冲区数据的底层 vector。
             * 包含实际的字节数据。
             */
            std::vector<uint8_t> data_vector_;
            /**
             * @brief 互斥锁。
             * 用于保护 data_vector_ 的并发访问，确保线程安全。
             */
            mutable std::mutex mutex_; // Thread safety mutex

        public:
            // Constructor/Destructor
            /**
             * @brief 默认构造函数。
             * 创建一个空的缓冲区。
             */
            Buffer(); // Empty buffer
            /**
             * @brief 构造函数。
             * 创建一个指定初始大小的缓冲区，内容未初始化。
             *
             * @param size 缓冲区的初始大小（字节）。
             */
            Buffer(size_t size); // Buffer with initial size
            /**
             * @brief 析构函数。
             * 默认析构函数，由 std::vector 自动管理内存释放。
             */
            ~Buffer() = default; // std::vector handles deallocation

            // Copy and assignment are fine for Buffer (vector handles deep copy)
            /**
             * @brief 拷贝构造函数。
             * 创建一个 Buffer 的拷贝，包含源缓冲区数据的深拷贝。
             */
            Buffer(const Buffer&) = default;
            /**
             * @brief 拷贝赋值运算符。
             * 将源 Buffer 的内容深拷贝到当前缓冲区。
             *
             * @param other 源 Buffer 对象。
             * @return 对当前 Buffer 对象的引用。
             */
            Buffer& operator=(const Buffer&) = default;
            /**
             * @brief 移动构造函数。
             * 从源 Buffer 移动资源到当前缓冲区。
             *
             * @param other 源 Buffer 对象。
             */
            Buffer(Buffer&&) = default;
            /**
             * @brief 移动赋值运算符。
             * 从源 Buffer 移动资源到当前缓冲区。
             *
             * @param other 源 Buffer 对象。
             * @return 对当前 Buffer 对象的引用。
             */
            Buffer& operator=(Buffer&&) = default;


            // --- Management Functions ---
            /**
             * @brief 调整缓冲区的大小。
             * 改变缓冲区的当前大小。如果新大小小于当前大小，多余的数据会被截断。
             * 如果新大小大于当前大小，新分配的部分内容未定义。
             *
             * @param new_size 缓冲区的新大小（字节）。
             * @return 如果成功调整大小，返回 true；如果分配内存失败，返回 false（或抛出 std::bad_alloc）。
             */
            bool Resize(size_t new_size);

            /**
             * @brief 清空缓冲区。
             * 将缓冲区的大小调整为 0。
             */
            void Clear();

            /**
             * @brief 填充缓冲区。
             * 使用指定的字节值填充整个缓冲区。
             *
             * @param value 用于填充的字节值。
             */
            void Fill(uint8_t value);


            // --- Data Access Functions ---
            /**
             * @brief 获取指向底层数据的指针。
             * 返回指向缓冲区第一个字节的指针。
             * **注意：** 获取指针后，用户需要自行保证对数据的访问是线程安全的，特别是在多线程环境中。
             *
             * @return 指向缓冲区数据的指针。如果缓冲区为空，返回 nullptr。
             */
            uint8_t* GetData();
            /**
             * @brief 获取指向底层数据的常量指针。
             * 返回指向缓冲区第一个字节的常量指针。用于只读访问。
             *
             * @return 指向缓冲区数据的常量指针。如果缓冲区为空，返回 nullptr。
             */
            const uint8_t* GetData() const;

            /**
             * @brief 在指定偏移量写入数据。
             * 将指定数据从给定的偏移量开始写入缓冲区。
             *
             * @param offset 写入的起始偏移量（从 0 开始）。
             * @param data 指向要写入数据的缓冲区。
             * @param size 要写入数据的字节数。
             * @return 实际写入的字节数。如果 offset 或 offset + size 超出缓冲区范围，返回 0。
             */
            size_t WriteAt(size_t offset, const uint8_t* data, size_t size);
            /**
             * @brief 在指定偏移量写入数据。
             * 将 std::vector 中的数据从给定的偏移量开始写入缓冲区。
             *
             * @param offset 写入的起始偏移量（从 0 开始）。
             * @param data 包含要写入数据的 std::vector。
             * @return 实际写入的字节数。如果 offset 或 offset + data.size() 超出缓冲区范围，返回 0。
             */
            size_t WriteAt(size_t offset, const std::vector<uint8_t>& data);


            /**
             * @brief 在指定偏移量读取数据。
             * 从缓冲区中指定偏移量开始读取数据到给定的缓冲区。
             *
             * @param offset 读取的起始偏移量（从 0 开始）。
             * @param buffer 指向用于存储读取数据的缓冲区。
             * @param size 要读取的最大字节数。
             * @return 实际读取的字节数。如果 offset 或 offset + size 超出缓冲区范围，返回 0。
             */
            size_t ReadAt(size_t offset, uint8_t* buffer, size_t size) const;
            /**
             * @brief 在指定偏移量读取数据。
             * 从缓冲区中指定偏移量开始读取指定数量的数据，并返回一个新的 std::vector。
             *
             * @param offset 读取的起始偏移量（从 0 开始）。
             * @param size 要读取的字节数。
             * @return 包含读取数据的 std::vector。如果 offset 或 offset + size 超出缓冲区范围，返回一个空的 vector。
             */
            std::vector<uint8_t> ReadAt(size_t offset, size_t size) const;


            // --- Status Functions ---
            /**
             * @brief 获取缓冲区的当前大小。
             *
             * @return 缓冲区的当前大小（字节）。
             */
            size_t GetSize() const;

            /**
             * @brief 检查缓冲区是否为空。
             *
             * @return 如果缓冲区大小为 0，返回 true；否则返回 false。
             */
            bool IsEmpty() const;

            /**
             * @brief 获取缓冲区的容量。
             * 对于 std::vector，容量是实际分配的内存大小，可能大于当前 size。
             * (注意：此实现基于 std::vector，Capacity 通常与 size 相同，除非 vector 预留了空间或增长策略不同)。
             *
             * @return 缓冲区的容量（字节）。
             */
            size_t Capacity() const;
        };

    } // namespace LIB_LSX::Memory


#endif // LIB_LSX_MEMORY_BUFFER_H
