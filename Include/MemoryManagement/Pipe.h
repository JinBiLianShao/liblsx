/**
 * @file Pipe.h
 * @brief 管道类 (模板)
 * @details 定义了 LIB_LSX::Memory 命名空间下的 Pipe 类，
 * 用于实现一个线程安全的、基于 std::deque 的数据传输管道，通常用于字节流（可变长度）。
 * 提供非阻塞和阻塞（带超时）的数据写入（Write/Put）和读取（Read/Get）操作。
 * 类内部使用 std::mutex 和 std::condition_variable 来保证在多线程环境下的线程安全访问和同步。
 * 此实现通常是无界的（受限于系统内存）。
 * 类禁用了拷贝和赋值，以避免线程同步状态的复杂性。
 * @author 连思鑫（liansixin）
 * @date 2025-5-7
 * @version 1.0
 *
 * ### 核心功能
 * - **可变长度数据**: 支持传输可变长度的数据块（字节流）。
 * - **基于 Deque**: 利用 `std::deque` 作为底层容器，支持高效地在两端进行读写。
 * - **非阻塞操作**: 提供 `Write`/`Put` 和 `Read`/`Get` 方法，在管道空时立即返回（对于 Read/Get）。
 * - **阻塞操作**: 提供 `WriteBlocking` 和 `ReadBlocking` 方法，支持无限等待、非阻塞或带超时等待。
 * - **线程安全**: 使用内部互斥锁 (`std::mutex`) 保护对管道状态和底层缓冲区的并发访问，使用条件变量 (`std::condition_variable`) 实现阻塞操作的线程同步。
 * - **状态查询**: 提供 `IsEmpty`, `Size` 方法查询管道状态。
 * - **Peek 操作**: 支持查看管道头部的数据而不将其移除。
 * - **资源管理**: RAII 模式，`std::deque` 自动管理内存。
 *
 * ### 使用示例
 *
 * @code
 * #include "Pipe.h"
 * #include <iostream>
 * #include <vector>
 * #include <thread>
 * #include <string>
 * #include <chrono>
 *
 * // 创建一个 Pipe 实例
 * LIB_LSX::Memory::Pipe byte_pipe;
 *
 * // 写入者线程
 * void writer(int id) {
 * for (int i = 0; i < 5; ++i) {
 * std::string message = "Hello " + std::to_string(i) + " from writer " + std::to_string(id) + "!";
 * std::vector<uint8_t> data(message.begin(), message.end());
 * std::cout << "[Writer " << id << "] Attempting to write " << data.size() << " bytes..." << std::endl;
 * // 使用阻塞 Write，对于无界管道，通常不会阻塞（除非内存耗尽）
 * size_t written = byte_pipe.WriteBlocking(data);
 * if (written > 0) {
 * std::cout << "[Writer " << id << "] Successfully wrote " << written << " bytes. Pipe size: " << byte_pipe.Size() << std::endl;
 * } else {
 * std::cerr << "[Writer " << id << "] Failed to write." << std::endl;
 * }
 * std::this_thread::sleep_for(std::chrono::milliseconds(50)); // 短暂休息
 * }
 * }
 *
 * // 读取者线程
 * void reader(int id) {
 * std::vector<uint8_t> buffer(50); // 接收缓冲区
 * for (int i = 0; i < 5; ++i) {
 * std::cout << "[Reader " << id << "] Attempting to read..." << std::endl;
 * // 使用阻塞 Read，等待管道有数据，最多等待 200ms
 * size_t read_bytes = byte_pipe.ReadBlocking(buffer.data(), buffer.size(), 200);
 * if (read_bytes > 0) {
 * std::string received_message(buffer.begin(), buffer.begin() + read_bytes);
 * std::cout << "[Reader " << id << "] Successfully read " << read_bytes << " bytes. Pipe size: " << byte_pipe.Size() << ". Data: \"" << received_message << "\"" << std::endl;
 * } else if (read_bytes == 0) {
 * std::cout << "[Reader " << id << "] Read timed out or pipe empty." << std::endl;
 * } else { // read_bytes < 0 indicates error in some potential future implementation
 * std::cerr << "[Reader " << id << "] Encountered an error during read." << std::endl;
 * }
 * std::this_thread::sleep_for(std::chrono::milliseconds(150)); // 模拟处理时间
 * }
 * }
 *
 * int main() {
 * std::cout << "Byte Pipe created." << std::endl;
 *
 * std::thread w1(writer, 1);
 * std::thread r1(reader, 1);
 *
 * w1.join();
 * r1.join();
 *
 * std::cout << "All threads finished." << std::endl;
 * std::cout << "Final pipe size: " << byte_pipe.Size() << std::endl;
 *
 * // 示例：使用 Peek
 * if (!byte_pipe.IsEmpty()) {
 * std::vector<uint8_t> peek_buffer(10);
 * size_t peeked_bytes = byte_pipe.Peek(peek_buffer.data(), peek_buffer.size());
 * if (peeked_bytes > 0) {
 * std::string peeked_data(peek_buffer.begin(), peek_buffer.begin() + peeked_bytes);
 * std::cout << "Peeked " << peeked_bytes << " bytes: \"" << peeked_data << "\"" << std::endl;
 * }
 * }
 *
 * // 示例：清空管道
 * byte_pipe.Clear();
 * std::cout << "Pipe cleared. IsEmpty: " << byte_pipe.IsEmpty() << std::endl;
 *
 * std::cout << "主线程退出。" << std::endl;
 * return 0;
 * }
 * @endcode
 *
 * ### 注意事项
 * - **无界**: 基于 std::deque，Pipe 通常是无界的。如果需要固定容量或阻塞写入直到有空间可用，需要修改实现并设置容量限制。当前的 WriteBlocking 在无界情况下行为与 Write 相同。
 * - **数据复制**: `Write`, `Read`, `Peek` 方法都涉及数据的复制。对于需要避免复制的场景，可能需要修改接口以返回指向内部缓冲区的指针（但需谨慎处理生命周期和线程安全）。
 * - **线程安全**: 所有公共方法都通过互斥锁保护，支持多线程访问。阻塞方法使用条件变量进行同步。
 * - **阻塞超时**: 阻塞方法的超时参数以毫秒为单位。超时为 -1 表示无限等待，0 表示非阻塞。
 * - **异常处理**: `Read` 和 `Peek` 在尝试读取超过可用数据时不会抛出异常，而是返回实际读取的字节数（可能为 0）。
 * - **拷贝/移动**: 类禁用了拷贝构造和赋值，因为直接拷贝一个包含线程同步原语（mutex, condition_variable）和状态的管道通常是不安全或没有意义的。如果需要传递管道对象，应考虑使用智能指针（如 `std::shared_ptr`）。移动语义未默认启用，如果需要，需要手动实现以安全转移资源和状态。
 */

#ifndef LIB_LSX_MEMORY_PIPE_H
#define LIB_LSX_MEMORY_PIPE_H
#pragma once
// 包含全局错误锁头文件
#include "GlobalErrorMutex.h"
// 包含 LIBLSX::LockManager::LockGuard 头文件
#include "LockGuard.h"
#include <deque> // For std::deque
#include <cstdint> // For uint8_t
#include <vector> // For std::vector
#include <optional> // For Peek methods (std::optional, C++17)
#include <mutex>   // For thread safety (std::mutex, std::lock_guard, std::unique_lock)
#include <condition_variable> // For blocking operations (std::condition_variable)
#include <algorithm> // For std::min
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
         * @brief 管道类。
         * 实现一个线程安全的、基于 std::deque 的数据传输管道，用于可变长度的字节流。
         * 支持非阻塞和阻塞（带超时）的写入和读取操作。通常是无界的。
         */
        // 2. Pipe Module (管道模块)
        // 实现数据传输通道，通常是字节流 (可变长度)
        // 接口优化：增加Clear, Peek, Blocking Read/Write interfaces
        // 线程安全：使用 std::mutex 和 std::condition_variable
        class Pipe {
        private:
            /**
             * @brief 存储管道数据的底层 deque。
             * 用于实现高效的双端读写。
             */
            std::deque<uint8_t> byte_stream_;
            /**
             * @brief 互斥锁。
             * 用于保护 byte_stream_ 的并发访问，确保线程安全。
             */
            mutable std::mutex mutex_; // For thread safety
            /**
             * @brief 条件变量，用于阻塞读取操作。
             * 当管道为空时，读取线程在此等待，直到有新数据写入。
             */
            std::condition_variable cv_read_; // For blocking reads
            /**
             * @brief 条件变量，用于阻塞写入操作。
             * 当管道满时（如果实现为有界管道），写入线程在此等待，直到有空间可用。
             * 在当前无界实现中，此条件变量主要用于概念上的完整性，实际等待不会发生。
             */
            std::condition_variable cv_write_; // For blocking writes (if bounded)
            // size_t capacity_ = std::numeric_limits<size_t>::max(); // Conceptual unbounded capacity
            // size_t capacity_ = 1024; // Example capacity for a bounded pipe


        public:
            /**
             * @brief 构造函数。
             * 初始化 Pipe 对象，创建一个空的字节流。
             */
            // Constructor/Destructor
            Pipe(); // Constructor definition will be in .cpp
            /**
             * @brief 析构函数。
             * 清理 Pipe 对象，由 std::deque 自动释放内存。
             */
            ~Pipe(); // Destructor definition will be in .cpp

            // Prevent copying and assignment
            /**
             * @brief 禁用拷贝构造函数。
             * Pipe 包含同步原语和状态，拷贝不安全。
             */
            Pipe(const Pipe&) = delete;
            /**
             * @brief 禁用拷贝赋值运算符。
             * Pipe 包含同步原语和状态，拷贝不安全。
             */
            Pipe& operator=(const Pipe&) = delete;
            // Consider adding move constructor/assignment

            // --- Management Functions ---
            /**
             * @brief 清空管道。
             * 移除管道中的所有数据。
             */
            // Clear the pipe stream
            void Clear();

            // --- Data Access Functions (Non-blocking) ---
            /**
             * @brief 将数据写入管道尾部 (非阻塞)。
             * 将指定数据复制到管道的末尾。
             * 在无界管道中，此操作总是成功（除非内存耗尽）。在有界管道中，如果管道满，可能只写入部分数据或返回 0。
             *
             * @param data 指向要写入数据的缓冲区。
             * @param size 要写入数据的字节数。
             * @return 实际写入的字节数。在无界管道中通常等于 size。在有界管道中，如果管道满，可能小于 size 或为 0。
             */
            // Writes data to the pipe (Put)
            // Returns number of bytes written (equal to size if successful, could be less in bounded pipe)
            size_t Write(const uint8_t* data, size_t size);
            /**
             * @brief 将 std::vector 中的数据写入管道尾部 (非阻塞)。
             * 便利方法，将 std::vector 中的数据复制到管道的末尾。
             *
             * @param data 包含要写入数据的 std::vector。
             * @return 实际写入的字节数。在无界管道中通常等于 data.size()。
             */
            size_t Write(const std::vector<uint8_t>& data); // Convenience method

            /**
             * @brief 将数据写入管道尾部 (非阻塞)。
             * 便利方法，别名 Write。
             *
             * @param data 指向要写入数据的缓冲区。
             * @param size 要写入数据的字节数。
             * @return 实际写入的字节数。
             */
            // Alias for Write
            size_t Put(const uint8_t* data, size_t size) { return Write(data, size); }
            /**
             * @brief 将 std::vector 中的数据写入管道尾部 (非阻塞)。
             * 便利方法，别名 Write。
             *
             * @param data 包含要写入数据的 std::vector。
             * @return 实际写入的字节数。
             */
            size_t Put(const std::vector<uint8_t>& data) { return Write(data); }


            /**
             * @brief 从管道头部读取数据 (非阻塞)。
             * 从管道头部读取数据到提供的缓冲区，并从管道中移除已读取的数据。
             * 如果管道为空，立即返回 0。如果管道中的数据少于请求的大小，会读取所有可用数据。
             *
             * @param buffer 指向用于存储读取数据的缓冲区。
             * @param size 要读取的最大字节数。
             * @return 实际读取的字节数。如果管道为空或没有数据可用，返回 0。如果管道中的数据少于 size，返回实际读取的字节数。
             */
            // Reads data from the pipe (Get)
            // Returns number of bytes read (could be less than size, or 0 if empty/no data available)
            size_t Read(uint8_t* buffer, size_t size);
            /**
             * @brief 从管道头部读取数据，并返回其拷贝 (非阻塞)。
             * 便利方法，从管道头部读取指定数量的数据，并返回一个新的 std::vector。
             *
             * @param size 要读取的字节数。
             * @return 包含读取数据的 std::vector。如果管道为空或数据不足，返回包含实际读取字节数的 vector。
             */
            std::vector<uint8_t> Read(size_t size); // Convenience method

            /**
             * @brief 从管道头部读取数据 (非阻塞)。
             * 便利方法，别名 Read。
             *
             * @param buffer 指向用于存储读取数据的缓冲区。
             * @param size 要读取的最大字节数。
             * @return 实际读取的字节数。
             */
            // Alias for Read
            size_t Get(uint8_t* buffer, size_t size) { return Read(buffer, size); }
            /**
             * @brief 从管道头部读取数据，并返回其拷贝 (非阻塞)。
             * 便利方法，别名 Read。
             *
             * @param size 要读取的字节数。
             * @return 包含读取数据的 std::vector。
             */
            std::vector<uint8_t> Get(size_t size) { return Read(size); }


            /**
             * @brief 查看管道头部的数据，但不移除 (非阻塞)。
             * 从管道头部复制数据到提供的缓冲区。管道保持不变。
             * 如果管道为空，立即返回 0。如果管道中的数据少于请求的大小，会复制所有可用数据。
             *
             * @param buffer 指向用于存储查看数据的缓冲区。
             * @param size 要查看的最大字节数。
             * @return 实际查看的字节数。如果管道为空或没有数据可用，返回 0。如果管道中的数据少于 size，返回实际查看的字节数。
             */
            // Peek data from the pipe without removing it
            // Returns number of bytes peeked (could be less than size, or 0 if empty)
            size_t Peek(uint8_t* buffer, size_t size) const;
            /**
             * @brief 查看管道头部的数据，并返回其拷贝，但不移除 (非阻塞)。
             * 便利方法，从管道头部复制指定数量的数据，并返回一个新的 std::vector。管道保持不变。
             *
             * @param size 要查看的字节数。
             * @return 包含查看数据的 std::vector。如果管道为空或数据不足，返回包含实际查看字节数的 vector。
             */
            std::vector<uint8_t> Peek(size_t size) const; // Convenience method


            // --- Data Access Functions (Blocking) ---
            /**
             * @brief 从管道头部读取数据 (阻塞)。
             * 从管道头部读取数据到提供的缓冲区，并从管道中移除已读取的数据。
             * 如果管道为空，线程将等待直到有数据可用，或直到超时发生。
             * 如果等待期间有数据可用，此方法会读取所有可用数据（最多不超过请求的 size），即使数据量小于 size。
             *
             * @param buffer 指向用于存储读取数据的缓冲区。
             * @param size 要读取的最大字节数。
             * @param timeout_ms 等待超时时间，单位为毫秒。
             * - < 0: 无限等待。
             * - == 0: 非阻塞（行为同非阻塞 Read）。
             * - > 0: 最多等待指定的毫秒数。
             * @return 实际读取的字节数。如果超时、管道为空（非阻塞模式）或没有数据可用，返回 0。如果成功读取，返回实际读取的字节数（可能小于 size）。
             */
            // Read data from the pipe, waits if empty.
            // Returns number of bytes read (could be less than requested size if read fully consumes pipe before timeout/notification, or 0 on timeout/empty).
            size_t ReadBlocking(uint8_t* buffer, size_t size, long timeout_ms = -1);
            /**
             * @brief 从管道头部读取数据，并返回其拷贝 (阻塞)。
             * 便利方法，从管道头部读取指定数量的数据，并返回一个新的 std::vector。
             * 如果管道为空，线程将等待直到有数据可用，或直到超时发生。
             *
             * @param size 要读取的字节数。
             * @param timeout_ms 等待超时时间，单位为毫秒。参见 ReadBlocking(uint8_t*, size_t, long) 的说明。
             * @return 包含读取数据的 std::vector。如果管道为空（且超时/非阻塞返回）或超时发生，返回一个空的 vector。
             */
            std::vector<uint8_t> ReadBlocking(size_t size, long timeout_ms = -1);


            /**
             * @brief 将数据写入管道尾部 (阻塞)。
             * 将指定数据复制到管道的末尾。
             * 在无界管道中，此操作总是成功（除非内存耗尽），不会因为管道满而阻塞。
             * 在有界管道中，如果管道满，线程将等待直到有空间可用，或直到超时发生。
             *
             * @param data 指向要写入数据的缓冲区。
             * @param size 要写入数据的字节数。
             * @param timeout_ms 等待超时时间，单位为毫秒。仅在有界管道中用于等待空间。
             * @return 实际写入的字节数。在无界管道中通常等于 size。在有界管道中，如果超时或管道满，可能小于 size 或为 0。
             */
            // Write data to the pipe, waits if full (for bounded pipes).
            // Returns number of bytes written (equal to size if successful, could be less on timeout/full).
            // Note: Current implementation is unbounded, so WriteBlocking will behave like Write.
            size_t WriteBlocking(const uint8_t* data, size_t size, long timeout_ms = -1);
            /**
             * @brief 将 std::vector 中的数据写入管道尾部 (阻塞)。
             * 便利方法，将 std::vector 中的数据复制到管道的末尾。
             * 在无界管道中，此操作总是成功。
             *
             * @param data 包含要写入数据的 std::vector。
             * @param timeout_ms 等待超时时间，单位为毫秒。仅在有界管道中用于等待空间。
             * @return 实际写入的字节数。在无界管道中通常等于 data.size()。
             */
            size_t WriteBlocking(const std::vector<uint8_t>& data, long timeout_ms = -1);


            // --- Status Functions ---
            /**
             * @brief 检查管道是否为空。
             *
             * @return 如果管道中当前没有数据，返回 true；否则返回 false。
             */
            // Check if the pipe stream is empty
            bool IsEmpty() const;

            /**
             * @brief 获取管道中当前存储的数据字节数。
             *
             * @return 管道中当前存储的数据字节数。
             */
            // Get the number of bytes currently in the pipe stream
            size_t Size() const;

            // Pipe is typically unbounded unless a capacity is set internally.
            // bool IsFull() const;     // If bounded (Not applicable for current unbounded implementation)
            // size_t Capacity() const; // If bounded (Not applicable for current unbounded implementation)
        };

    } // namespace Memory
} // namespace LIB_LSX

#endif // LIB_LSX_MEMORY_PIPE_H
