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

#include <vector>
#include <cstdint> // For uint8_t
#include <string>
#include <mutex> // Still needed for std::mutex definition
#include <stdexcept> // For exceptions

// 引入我们自定义的锁管理模块
#include <cstring>

#include "LockGuard.h" // 用于 LIBLSX::LockManager::LockGuard

namespace LIB_LSX {
namespace Memory {

class Buffer {
public:
    // 构造函数和析构函数
    explicit Buffer(size_t initial_size = 0);
    Buffer(const uint8_t* data, size_t size);
    Buffer(const Buffer& other); // 拷贝构造函数
    Buffer(Buffer&& other) noexcept; // 移动构造函数
    Buffer& operator=(const Buffer& other); // 拷贝赋值运算符
    Buffer& operator=(Buffer&& other) noexcept; // 移动赋值运算符
    ~Buffer();

    // 数据访问
    // 警告: 返回原始指针意味着调用者需要自行确保线程安全
    // 如果在 Buffer 的加锁方法外部进行直接访问，请务必小心。
    uint8_t* Data();
    const uint8_t* Data() const;
    size_t Size() const;

    // 操作 - 所有这些方法都将在内部处理锁管理
    bool WriteAt(size_t offset, const uint8_t* data, size_t size);
    bool ReadAt(size_t offset, uint8_t* data, size_t size) const;
    bool Resize(size_t new_size);
    void Clear();
    void Fill(uint8_t value);

    // 便利方法 - 这些模板方法也将在内部处理锁管理
    template<typename T>
    bool Write(size_t offset, const T& value);

    template<typename T>
    bool Read(size_t offset, T& value) const;

    // 获取字符串表示 (用于调试/显示)
    std::string ToString() const;

private:
    std::vector<uint8_t> buffer_;
    mutable std::mutex mutex_; // 用于线程安全，mutable 允许 const 方法加锁
};

// 模板方法的实现 (必须在头文件中，除非显式实例化)
// 这些方法内部使用 LIBLSX::LockManager::LockGuard 进行保护。
template<typename T>
bool Buffer::Write(size_t offset, const T& value) {
    // 使用我们自定义的 LockGuard 进行 RAII 保护
    LIBLSX::LockManager::LockGuard<std::mutex> lock(mutex_);
    if (offset + sizeof(T) > buffer_.size()) {
        return false;
    }
    std::memcpy(buffer_.data() + offset, &value, sizeof(T));
    return true;
}

template<typename T>
bool Buffer::Read(size_t offset, T& value) const {
    // 使用我们自定义的 LockGuard 进行 RAII 保护
    LIBLSX::LockManager::LockGuard<std::mutex> lock(mutex_);
    if (offset + sizeof(T) > buffer_.size()) {
        return false;
    }
    std::memcpy(&value, buffer_.data() + offset, sizeof(T));
    return true;
}

} // namespace Memory
} // namespace LIB_LSX
#endif // LIB_LSX_MEMORY_BUFFER_H
