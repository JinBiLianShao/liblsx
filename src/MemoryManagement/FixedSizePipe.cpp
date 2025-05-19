#pragma once
#include "FixedSizePipe.h"

#include <cstring>
#include <algorithm>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <iostream>
// #include <iostream>
#include "LockGuard.h"
#include "MultiLockGuard.h"


namespace LIB_LSX {
namespace Memory {

// Helper implementations (can be private members or free functions if in same cpp)
uint8_t* FixedSizePipe::get_block_address(size_t index) {
     // No lock here, assumes caller holds the lock
    if (index >= block_count_) {
         // This should not happen if head_ and tail_ are managed correctly
         std::cerr << "FixedSizePipe: get_block_address called with out-of-bounds index: " << index << std::endl;
         throw std::out_of_range("FixedSizePipe: Block index out of range in get_block_address.");
    }
    return buffer_.data() + index * block_size_;
}

const uint8_t* FixedSizePipe::get_block_address(size_t index) const {
    // No lock here, assumes caller holds the lock
    if (index >= block_count_) {
         std::cerr << "FixedSizePipe: get_block_address (const) called with out-of-bounds index: " << index << std::endl;
         throw std::out_of_range("FixedSizePipe: Block index out of range in get_block_address (const).");
    }
    return buffer_.data() + index * block_size_;
}


FixedSizePipe::FixedSizePipe(size_t block_size, size_t block_count)
    : block_size_(block_size), block_count_(block_count), head_(0), tail_(0), current_size_(0)
{
    if (block_size == 0 || block_count == 0) {
        throw std::invalid_argument("FixedSizePipe: block_size and block_count must be greater than 0");
    }
    try {
        buffer_.resize(block_size_ * block_count_);
        // std::cout << "FixedSizePipe: Created with block_size=" << block_size_
        //           << ", block_count=" << block_count_
        //           << ", total_size=" << buffer_.size() << std::endl;
    } catch (const std::bad_alloc& e) {
        std::cerr << "FixedSizePipe: Failed to allocate buffer: " << e.what() << std::endl;
        buffer_.clear();
        block_size_ = 0;
        block_count_ = 0;
        throw;
    }
}

void FixedSizePipe::Clear() {
    LSX_LIB::LockManager::LockGuard<std::mutex> lock(mutex_);
    head_ = 0;
    tail_ = 0;
    current_size_ = 0;
    // std::cout << "FixedSizePipe: Cleared." << std::endl;
    // 通知所有等待的写入者和读取者，因为状态已改变
    cv_write_.notify_all();
    cv_read_.notify_all();
}

bool FixedSizePipe::Write(const uint8_t* data, size_t data_size) {
    if (data == nullptr || data_size != block_size_) {
        // std::cerr << "FixedSizePipe: Write failed. Invalid data or size mismatch." << std::endl;
        return false;
    }
    LSX_LIB::LockManager::LockGuard<std::mutex> lock(mutex_);

    if (current_size_ == block_count_) { // 直接检查 current_size_，因为已持有锁
        // std::cout << "FixedSizePipe: Pipe is full, cannot Write." << std::endl;
        return false;
    }

    uint8_t* dest = get_block_address(tail_);
    std::memcpy(dest, data, block_size_);

    tail_ = (tail_ + 1) % block_count_;
    current_size_++;

    // std::cout << "FixedSizePipe: Wrote block. New tail: " << tail_ << ", New size: " << current_size_ << std::endl;
    cv_read_.notify_one();
    return true;
}

bool FixedSizePipe::Write(const std::vector<uint8_t>& data) {
    return Write(data.data(), data.size());
}


bool FixedSizePipe::Read(uint8_t* buffer, size_t buffer_size) {
    if (buffer == nullptr || buffer_size < block_size_) {
        // std::cerr << "FixedSizePipe: Read failed. Invalid buffer or size too small." << std::endl;
        return false;
    }
    LSX_LIB::LockManager::LockGuard<std::mutex> lock(mutex_);

    if (current_size_ == 0) { // 直接检查 current_size_，因为已持有锁
        // std::cout << "FixedSizePipe: Pipe is empty, cannot Read." << std::endl;
        return false;
    }

    const uint8_t* src = get_block_address(head_);
    std::memcpy(buffer, src, block_size_);

    head_ = (head_ + 1) % block_count_;
    current_size_--;

    // std::cout << "FixedSizePipe: Read block. New head: " << head_ << ", New size: " << current_size_ << std::endl;
    cv_write_.notify_one();
    return true;
}

std::optional<std::vector<uint8_t>> FixedSizePipe::Read() {
    LSX_LIB::LockManager::LockGuard<std::mutex> lock(mutex_);
    if (current_size_ == 0) { // 直接检查
        // std::cout << "FixedSizePipe: Pipe is empty, cannot Read (vector)." << std::endl;
        return std::nullopt;
    }

    std::vector<uint8_t> block_data(block_size_);
    const uint8_t* src = get_block_address(head_);
    std::memcpy(block_data.data(), src, block_size_);

    head_ = (head_ + 1) % block_count_;
    current_size_--;

    // std::cout << "FixedSizePipe: Read block (vector). New head: " << head_ << ", New size: " << current_size_ << std::endl;
    cv_write_.notify_one();
    return block_data;
}


bool FixedSizePipe::Peek(uint8_t* buffer, size_t buffer_size) const {
     if (buffer == nullptr || buffer_size < block_size_) {
        // std::cerr << "FixedSizePipe: Peek failed. Invalid buffer or size too small." << std::endl;
        return false;
    }
    LSX_LIB::LockManager::LockGuard<std::mutex> lock(mutex_);

    if (current_size_ == 0) { // 直接检查
        // std::cout << "FixedSizePipe: Pipe is empty, cannot Peek." << std::endl;
        return false;
    }

    const uint8_t* src = get_block_address(head_);
    std::memcpy(buffer, src, block_size_);

    // std::cout << "FixedSizePipe: Peeked block at head: " << head_ << std::endl;
    return true;
}

std::optional<std::vector<uint8_t>> FixedSizePipe::Peek() const {
    LSX_LIB::LockManager::LockGuard<std::mutex> lock(mutex_);
    if (current_size_ == 0) { // 直接检查
        // std::cout << "FixedSizePipe: Pipe is empty, cannot Peek (vector)." << std::endl;
        return std::nullopt;
    }

    std::vector<uint8_t> block_data(block_size_);
    const uint8_t* src = get_block_address(head_);
    std::memcpy(block_data.data(), src, block_size_);

    // std::cout << "FixedSizePipe: Peeked block (vector) at head: " << head_ << std::endl;
    return block_data;
}


// --- Data Access Functions (Blocking) ---
bool FixedSizePipe::ReadBlocking(uint8_t* buffer, size_t buffer_size, long timeout_ms) {
     if (buffer == nullptr || buffer_size < block_size_) {
        // std::cerr << "FixedSizePipe: ReadBlocking failed. Invalid buffer or size mismatch." << std::endl;
        return false;
    }
    std::unique_lock<std::mutex> lock(mutex_);

    // Lambda 谓词直接访问 current_size_
    auto pipe_not_empty = [&]{ return current_size_ != 0; };

    if (!pipe_not_empty()) { // 初始检查 (锁已持有)
        if (timeout_ms == 0) {
             // std::cout << "FixedSizePipe: ReadBlocking (non-blocking) empty." << std::endl;
            return false;
        } else if (timeout_ms > 0) {
            auto duration = std::chrono::milliseconds(timeout_ms);
            if (!cv_read_.wait_for(lock, duration, pipe_not_empty)) {
                 // std::cout << "FixedSizePipe: ReadBlocking timed out." << std::endl;
                 return false;
            }
        } else {
             // std::cout << "FixedSizePipe: ReadBlocking waiting indefinitely." << std::endl;
            cv_read_.wait(lock, pipe_not_empty);
        }
         // 重新检查条件，防止虚假唤醒后条件仍然不满足
         if (!pipe_not_empty()) {
             // std::cout << "FixedSizePipe: ReadBlocking woke up but still empty." << std::endl;
             return false;
         }
    }

    const uint8_t* src = get_block_address(head_);
    std::memcpy(buffer, src, block_size_);

    head_ = (head_ + 1) % block_count_;
    current_size_--;

    // std::cout << "FixedSizePipe: ReadBlocking got block. New head: " << head_ << ", New size: " << current_size_ << std::endl;

    // 解锁后通知，以减少锁争用
    lock.unlock();
    cv_write_.notify_one();
    return true;
}

std::optional<std::vector<uint8_t>> FixedSizePipe::ReadBlocking(long timeout_ms) {
    std::unique_lock<std::mutex> lock(mutex_);

    auto pipe_not_empty = [&]{ return current_size_ != 0; };

    if (!pipe_not_empty()) {
        if (timeout_ms == 0) {
            // std::cout << "FixedSizePipe: ReadBlocking (non-blocking, vector) empty." << std::endl;
            return std::nullopt;
        } else if (timeout_ms > 0) {
            auto duration = std::chrono::milliseconds(timeout_ms);
            if (!cv_read_.wait_for(lock, duration, pipe_not_empty)) {
                 // std::cout << "FixedSizePipe: ReadBlocking (vector) timed out." << std::endl;
                 return std::nullopt;
            }
        } else {
             // std::cout << "FixedSizePipe: ReadBlocking (vector) waiting indefinitely." << std::endl;
            cv_read_.wait(lock, pipe_not_empty);
        }
         if (!pipe_not_empty()) {
             // std::cout << "FixedSizePipe: ReadBlocking (vector) woke up but still empty." << std::endl;
             return std::nullopt;
         }
    }

    std::vector<uint8_t> block_data(block_size_);
    const uint8_t* src = get_block_address(head_);
    std::memcpy(block_data.data(), src, block_size_);

    head_ = (head_ + 1) % block_count_;
    current_size_--;

    // std::cout << "FixedSizePipe: ReadBlocking got block (vector). New head: " << head_ << ", New size: " << current_size_ << std::endl;
    lock.unlock();
    cv_write_.notify_one();
    return block_data;
}

bool FixedSizePipe::WriteBlocking(const uint8_t* data, size_t data_size, long timeout_ms) {
     if (data == nullptr || data_size != block_size_) {
        // std::cerr << "FixedSizePipe: WriteBlocking failed. Invalid data or size mismatch." << std::endl;
        return false;
    }
    std::unique_lock<std::mutex> lock(mutex_);

    auto pipe_not_full = [&]{ return current_size_ != block_count_; };

    if (!pipe_not_full()) { // 初始检查 (锁已持有)
        if (timeout_ms == 0) {
             // std::cout << "FixedSizePipe: WriteBlocking (non-blocking) full." << std::endl;
            return false;
        } else if (timeout_ms > 0) {
            auto duration = std::chrono::milliseconds(timeout_ms);
            if (!cv_write_.wait_for(lock, duration, pipe_not_full)) {
                 // std::cout << "FixedSizePipe: WriteBlocking timed out." << std::endl;
                 return false;
            }
        } else {
             // std::cout << "FixedSizePipe: WriteBlocking waiting indefinitely." << std::endl;
            cv_write_.wait(lock, pipe_not_full);
        }
        if (!pipe_not_full()) {
            // std::cout << "FixedSizePipe: WriteBlocking woke up but still full." << std::endl;
            return false;
        }
    }

    uint8_t* dest = get_block_address(tail_);
    std::memcpy(dest, data, block_size_);

    tail_ = (tail_ + 1) % block_count_;
    current_size_++;

    // std::cout << "FixedSizePipe: WriteBlocking put block. New tail: " << tail_ << ", New size: " << current_size_ << std::endl;
    lock.unlock();
    cv_read_.notify_one();
    return true;
}

bool FixedSizePipe::WriteBlocking(const std::vector<uint8_t>& data, long timeout_ms) {
    return WriteBlocking(data.data(), data.size(), timeout_ms);
}


// --- Status Functions ---
bool FixedSizePipe::IsEmpty() const {
    LSX_LIB::LockManager::LockGuard<std::mutex> lock(mutex_);
    return current_size_ == 0;
}

bool FixedSizePipe::IsFull() const {
    LSX_LIB::LockManager::LockGuard<std::mutex> lock(mutex_);
    return current_size_ == block_count_;
}

size_t FixedSizePipe::Size() const {
    LSX_LIB::LockManager::LockGuard<std::mutex> lock(mutex_);
    return current_size_;
}

} // namespace Memory
} // namespace LIB_LSX