#pragma once
#include "CircularFixedSizeQueue.h"

#include <cstring>
#include <algorithm>
#include <iostream>
#include <vector>
#include <optional>
#include <stdexcept>

#include "LockGuard.h"  // LSX_LIB::LockManager::LockGuard

namespace LIB_LSX {
namespace Memory {

// --- helper (assumes外部已锁定) ---
uint8_t* CircularFixedSizeQueue::get_block_address(size_t index) {
    if (index >= block_count_) {
        throw std::out_of_range("CircularFixedSizeQueue: Block index out of range");
    }
    return buffer_.data() + index * block_size_;
}
const uint8_t* CircularFixedSizeQueue::get_block_address(size_t index) const {
    if (index >= block_count_) {
        throw std::out_of_range("CircularFixedSizeQueue: Block index out of range");
    }
    return buffer_.data() + index * block_size_;
}

CircularFixedSizeQueue::CircularFixedSizeQueue(size_t block_size, size_t block_count)
    : block_size_(block_size)
    , block_count_(block_count)
    , head_(0)
    , tail_(0)
    , current_size_(0)
{
    if (block_size_ == 0 || block_count_ == 0) {
        throw std::invalid_argument("block_size and block_count must be > 0");
    }
    try {
        buffer_.resize(block_size_ * block_count_);
    } catch (const std::bad_alloc& e) {
        std::cerr << "Allocation failed: " << e.what() << std::endl;
        block_size_ = block_count_ = current_size_ = head_ = tail_ = 0;
        buffer_.clear();
        throw;
    }
}

void CircularFixedSizeQueue::Clear() {
    // RAII 锁
    LSX_LIB::LockManager::LockGuard<std::mutex> guard(mutex_);
    head_ = tail_ = current_size_ = 0;
    cv_write_.notify_all();
    cv_read_.notify_all();
}

bool CircularFixedSizeQueue::Put(const uint8_t* data, size_t data_size) {
    if (!data || data_size != block_size_) return false;

    LSX_LIB::LockManager::LockGuard<std::mutex> guard(mutex_);

    if (current_size_ == block_count_) return false;

    std::memcpy(get_block_address(tail_), data, block_size_);
    tail_ = (tail_ + 1) % block_count_;
    ++current_size_;

    cv_read_.notify_one();
    return true;
}

bool CircularFixedSizeQueue::Put(const std::vector<uint8_t>& data) {
    return Put(data.data(), data.size());
}

bool CircularFixedSizeQueue::Get(uint8_t* buffer, size_t buffer_size) {
    if (!buffer || buffer_size < block_size_) return false;

    LSX_LIB::LockManager::LockGuard<std::mutex> guard(mutex_);

    if (current_size_ == 0) return false;

    std::memcpy(buffer, get_block_address(head_), block_size_);
    head_ = (head_ + 1) % block_count_;
    --current_size_;

    cv_write_.notify_one();
    return true;
}

std::optional<std::vector<uint8_t>> CircularFixedSizeQueue::Get() {
    LSX_LIB::LockManager::LockGuard<std::mutex> guard(mutex_);

    if (current_size_ == 0) return std::nullopt;

    std::vector<uint8_t> out(block_size_);
    std::memcpy(out.data(), get_block_address(head_), block_size_);
    head_ = (head_ + 1) % block_count_;
    --current_size_;

    cv_write_.notify_one();
    return out;
}

bool CircularFixedSizeQueue::Peek(uint8_t* buffer, size_t buffer_size) const {
    if (!buffer || buffer_size < block_size_) return false;

    // const 方法若需要锁，建议先在外部加锁再调用
    LSX_LIB::LockManager::LockGuard<std::mutex> guard(mutex_);

    if (current_size_ == 0) return false;

    std::memcpy(buffer, get_block_address(head_), block_size_);
    return true;
}

std::optional<std::vector<uint8_t>> CircularFixedSizeQueue::Peek() const {
    LSX_LIB::LockManager::LockGuard<std::mutex> guard(mutex_);

    if (current_size_ == 0) return std::nullopt;

    std::vector<uint8_t> out(block_size_);
    std::memcpy(out.data(), get_block_address(head_), block_size_);
    return out;
}

bool CircularFixedSizeQueue::GetBlocking(uint8_t* buffer, size_t buffer_size, long timeout_ms) {
    if (!buffer || buffer_size < block_size_) return false;

    std::unique_lock<std::mutex> lock(mutex_);
    auto not_empty = [&]{ return current_size_ != 0; };

    if (!cv_read_.wait_for(lock, std::chrono::milliseconds(timeout_ms > 0 ? timeout_ms : 0), not_empty))
        return false;

    if (current_size_ == 0) return false;
    std::memcpy(buffer, get_block_address(head_), block_size_);
    head_ = (head_ + 1) % block_count_;
    --current_size_;
    cv_write_.notify_one();
    return true;
}

std::optional<std::vector<uint8_t>> CircularFixedSizeQueue::GetBlocking(long timeout_ms) {
    std::unique_lock<std::mutex> lock(mutex_);
    auto not_empty = [&]{ return current_size_ != 0; };

    if (!cv_read_.wait_for(lock, std::chrono::milliseconds(timeout_ms > 0 ? timeout_ms : 0), not_empty))
        return std::nullopt;

    if (current_size_ == 0) return std::nullopt;
    std::vector<uint8_t> out(block_size_);
    std::memcpy(out.data(), get_block_address(head_), block_size_);
    head_ = (head_ + 1) % block_count_;
    --current_size_;
    cv_write_.notify_one();
    return out;
}

bool CircularFixedSizeQueue::PutBlocking(const uint8_t* data, size_t data_size, long timeout_ms) {
    if (!data || data_size != block_size_) return false;

    std::unique_lock<std::mutex> lock(mutex_);
    auto not_full = [&]{ return current_size_ != block_count_; };

    if (!cv_write_.wait_for(lock, std::chrono::milliseconds(timeout_ms > 0 ? timeout_ms : 0), not_full))
        return false;

    std::memcpy(get_block_address(tail_), data, block_size_);
    tail_ = (tail_ + 1) % block_count_;
    ++current_size_;
    cv_read_.notify_one();
    return true;
}

bool CircularFixedSizeQueue::PutBlocking(const std::vector<uint8_t>& data, long timeout_ms) {
    return PutBlocking(data.data(), data.size(), timeout_ms);
}

// 注意：IsEmpty/IsFull/Size 应由外部在持锁时调用，或在内部短暂加锁后立即返回
bool CircularFixedSizeQueue::IsEmpty() const {
    LSX_LIB::LockManager::LockGuard<std::mutex> guard(mutex_);
    return current_size_ == 0;
}

bool CircularFixedSizeQueue::IsFull() const {
    LSX_LIB::LockManager::LockGuard<std::mutex> guard(mutex_);
    return current_size_ == block_count_;
}

size_t CircularFixedSizeQueue::Size() const {
    LSX_LIB::LockManager::LockGuard<std::mutex> guard(mutex_);
    return current_size_;
}

} // namespace Memory
} // namespace LIB_LSX
