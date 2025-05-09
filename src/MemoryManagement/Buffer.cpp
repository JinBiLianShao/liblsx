#include "Buffer.h"

#include <cstring>      // For memcpy
#include <algorithm>    // For std::min, std::fill
#include <iostream>     // For debugging output (std::cerr) - 建议在生产环境中使用日志库
#include <stdexcept>    // For exceptions
#include <cstdio>       // For snprintf

// 引入我们自定义的锁管理模块
#include "LockGuard.h"
#include "MultiLockGuard.h" // 用于 Buffer::operator= (拷贝赋值)

namespace LIB_LSX {
namespace Memory {

// 构造函数
Buffer::Buffer(size_t initial_size) : buffer_(initial_size) {
    // 成员初始化不需要加锁
    // std::cout << "Buffer: Created with size " << initial_size << std::endl;
}

// 从数据构造的构造函数
Buffer::Buffer(const uint8_t* data, size_t size) : buffer_(size) {
    // 成员初始化不需要加锁
    if (data) {
        std::memcpy(buffer_.data(), data, size);
    }
    // std::cout << "Buffer: Created from data with size " << size << std::endl;
}

// 拷贝构造函数
Buffer::Buffer(const Buffer& other) {
    // 使用我们自定义的 LockGuard 安全地拷贝 'other' 的内部状态
    LIBLSX::LockManager::LockGuard<std::mutex> lock_other(other.mutex_);
    buffer_ = other.buffer_;
    // std::cout << "Buffer: Copied." << std::endl;
}

// 移动构造函数
// 移动操作通常不需要加锁，因为假定 'other' 在移动后不再被并发访问。
Buffer::Buffer(Buffer&& other) noexcept : buffer_(std::move(other.buffer_)) {
    // std::cout << "Buffer: Moved." << std::endl;
}

// 拷贝赋值运算符
Buffer& Buffer::operator=(const Buffer& other) {
    if (this != &other) {
        // 使用我们自定义的 MultiLockGuard 原子性地获取 'this' 和 'other' 的锁
        // 这可以防止当其他线程也尝试赋值时发生死锁。
        LIBLSX::LockManager::MultiLockGuard<std::mutex, std::mutex> lock_pair(mutex_, other.mutex_);

        buffer_ = other.buffer_;
    }
    // std::cout << "Buffer: Copy assigned." << std::endl;
    return *this;
}

// 移动赋值运算符
// 移动操作通常不需要加锁，因为假定 'other' 在移动后不再被并发访问。
Buffer& Buffer::operator=(Buffer&& other) noexcept {
    if (this != &other) {
        buffer_ = std::move(other.buffer_);
    }
    // std::cout << "Buffer: Move assigned." << std::endl;
    return *this;
}

Buffer::~Buffer() {
    // 成员清理不需要加锁，因为互斥量本身也在被销毁。
    // std::cout << "Buffer: Destroyed." << std::endl;
}

// 数据访问
// 警告: 这些方法返回原始指针。如果调用者通过此指针在 Buffer 加锁方法外部进行修改或读取，
// 可能会导致数据竞争。此处假定调用者在使用这些原始指针时会确保线程安全，
// 或它们仅在其他已加锁的 Buffer 方法内部使用，以避免潜在的嵌套加锁问题。
uint8_t* Buffer::Data() {
    // 此处不加内部锁，以避免与调用方可能持有的锁发生嵌套死锁。
    return buffer_.data();
}

const uint8_t* Buffer::Data() const {
    // 此处不加内部锁。原因同上。
    return buffer_.data();
}

size_t Buffer::Size() const {
    // 使用我们自定义的 LockGuard 保护对 buffer_ 大小的访问。
    LIBLSX::LockManager::LockGuard<std::mutex> lock(mutex_);
    return buffer_.size();
}

// 操作 - 所有这些方法都在内部处理锁管理
bool Buffer::WriteAt(size_t offset, const uint8_t* data, size_t size) {
    // 使用我们自定义的 LockGuard 保护内部状态。
    LIBLSX::LockManager::LockGuard<std::mutex> lock(mutex_);
    if (offset + size > buffer_.size()) {
        std::cerr << "Buffer: WriteAt failed. Offset + size exceeds buffer size." << std::endl;
        return false;
    }
    if (data) {
        std::memcpy(buffer_.data() + offset, data, size);
    }
    return true;
}

bool Buffer::ReadAt(size_t offset, uint8_t* data, size_t size) const {
    // 使用我们自定义的 LockGuard 保护内部状态。
    LIBLSX::LockManager::LockGuard<std::mutex> lock(mutex_);
    if (offset + size > buffer_.size()) {
        std::cerr << "Buffer: ReadAt failed. Offset + size exceeds buffer size." << std::endl;
        return false;
    }
    if (data) {
        std::memcpy(data, buffer_.data() + offset, size);
    }
    return true;
}

bool Buffer::Resize(size_t new_size) {
    // 使用我们自定义的 LockGuard 保护内部状态。
    LIBLSX::LockManager::LockGuard<std::mutex> lock(mutex_);
    try {
        buffer_.resize(new_size);
        // std::cout << "Buffer: Resized to " << new_size << std::endl;
        return true;
    } catch (const std::bad_alloc& e) {
        std::cerr << "Buffer: Resize failed due to memory allocation error: " << e.what() << std::endl;
        return false;
    } catch (...) {
        std::cerr << "Buffer: Resize failed due to unknown error." << std::endl;
        return false;
    }
}

void Buffer::Clear() {
    // 使用我们自定义的 LockGuard 保护内部状态。
    LIBLSX::LockManager::LockGuard<std::mutex> lock(mutex_);
    buffer_.clear();
    // std::cout << "Buffer: Cleared." << std::endl;
}

void Buffer::Fill(uint8_t value) {
    // 使用我们自定义的 LockGuard 保护内部状态。
    LIBLSX::LockManager::LockGuard<std::mutex> lock(mutex_);
    std::fill(buffer_.begin(), buffer_.end(), value);
    // std::cout << "Buffer: Filled with value " << static_cast<int>(value) << std::endl;
}

// 注意: `Buffer.h` 中公共的 `Lock()` 和 `Unlock()` 方法已被移除，
// 因为它们违反了 RAII 原则。使用者应依赖 Buffer 内部通过 LockManager 封装提供的加锁机制，
// 或者如果需要进行复杂的自定义操作，可以对 Buffer 实例的互斥量获取外部锁。

// 获取字符串表示 (用于调试/显示)
std::string Buffer::ToString() const {
    // 使用我们自定义的 LockGuard 保护内部状态。
    LIBLSX::LockManager::LockGuard<std::mutex> lock(mutex_);
    std::string s;
    s.reserve(buffer_.size() * 3); // 估计每字节需要两个字符加一个空格
    char buf[4]; // 用于 snprintf 的缓冲区

    for (size_t i = 0; i < buffer_.size(); ++i) {
        // 使用 snprintf 更安全地格式化单个字节为十六进制字符串
        snprintf(buf, sizeof(buf), "%02X ", buffer_[i]);
        s += buf;
    }
    // 移除末尾可能存在的空格
    if (!s.empty() && s.back() == ' ') {
        s.pop_back();
    }
    return s;
}

} // namespace Memory
} // namespace LIB_LSX