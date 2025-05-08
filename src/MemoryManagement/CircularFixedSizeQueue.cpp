#include "CircularFixedSizeQueue.h"

#include <cstring> // For memcpy
#include <algorithm> // For std::min, std::copy
#include <mutex> // For std::mutex, std::lock_guard, std::unique_lock
#include <condition_variable> // For std::condition_variable
#include <chrono> // For std::chrono::milliseconds
#include <iostream>
// #include <iostream>  // For example output - prefer logging


namespace LIB_LSX {
namespace Memory {

// Helper implementations (can be private members or free functions if in same cpp)
uint8_t* CircularFixedSizeQueue::get_block_address(size_t index) {
    // No lock here, assumes caller holds the lock
    if (index >= block_count_) {
         throw std::out_of_range("CircularFixedSizeQueue: Block index out of range");
    }
    return buffer_.data() + index * block_size_;
}

const uint8_t* CircularFixedSizeQueue::get_block_address(size_t index) const {
    // No lock here, assumes caller holds the lock
    if (index >= block_count_) {
         throw std::out_of_range("CircularFixedSizeQueue: Block index out of range");
    }
    return buffer_.data() + index * block_size_;
}


CircularFixedSizeQueue::CircularFixedSizeQueue(size_t block_size, size_t block_count)
    : block_size_(block_size), block_count_(block_count)
{
    if (block_size == 0 || block_count == 0) {
        throw std::invalid_argument("CircularFixedSizeQueue: block_size and block_count must be greater than 0");
    }
     try {
        // Lock during construction if shared state could be accessed before constructor finishes,
        // but typically not necessary as object is not fully constructed/shared yet.
        buffer_.resize(block_size_ * block_count_);
        // std::cout << "CircularFixedSizeQueue: Created with block_size=" << block_size_
        //           << ", block_count=" << block_count_
        //           << ", total_size=" << buffer_.size() << std::endl; // Use logging
    } catch (const std::bad_alloc& e) {
        std::cerr << "CircularFixedSizeQueue: Failed to allocate buffer: " << e.what() << std::endl;
        // Ensure object is in a safe state even if allocation fails
        buffer_.clear(); // Ensure vector is empty
        block_size_ = 0;
        block_count_ = 0;
        current_size_ = 0;
        head_ = 0;
        tail_ = 0;
        throw; // Re-throw the exception
    }
}

void CircularFixedSizeQueue::Clear() {
    std::lock_guard<std::mutex> lock(mutex_); // Thread safe clear
    head_ = 0;
    tail_ = 0;
    current_size_ = 0;
    // Data is not zeroed or erased, just logically reset pointers
    // std::cout << "CircularFixedSizeQueue: Cleared." << std::endl; // Use logging
    cv_write_.notify_all(); // Notify potential waiting writers that space is available
    cv_read_.notify_all(); // Notify potential waiting readers (they will read 0 blocks)
}

bool CircularFixedSizeQueue::Put(const uint8_t* data, size_t data_size) {
    if (data == nullptr || data_size != block_size_) {
        // std::cerr << "CircularFixedSizeQueue: Put failed. Invalid data or size mismatch." << std::endl; // Use logging
        return false;
    }
    std::lock_guard<std::mutex> lock(mutex_); // Thread safe Put

    if (IsFull()) {
        // std::cout << "CircularFixedSizeQueue: Queue is full, cannot Put." << std::endl; // Use logging
        return false;
    }

    uint8_t* dest = get_block_address(tail_);
    std::memcpy(dest, data, block_size_); // Copy the block data

    tail_ = (tail_ + 1) % block_count_; // Move tail circularly
    current_size_++;

    // std::cout << "CircularFixedSizeQueue: Put block at index " << (tail_ - 1 + block_count_) % block_count_ << std::endl; // Use logging
    cv_read_.notify_one(); // Notify one waiting reader that data is available
    return true;
}

bool CircularFixedSizeQueue::Put(const std::vector<uint8_t>& data) {
     return Put(data.data(), data.size());
}


bool CircularFixedSizeQueue::Get(uint8_t* buffer, size_t buffer_size) {
    if (buffer == nullptr || buffer_size < block_size_) {
        // std::cerr << "CircularFixedSizeQueue: Get failed. Invalid buffer or size too small." << std::endl; // Use logging
        return false;
    }
    std::lock_guard<std::mutex> lock(mutex_); // Thread safe Get

    if (IsEmpty()) {
        // std::cout << "CircularFixedSizeQueue: Queue is empty, cannot Get." << std::endl; // Use logging
        return false;
    }

    const uint8_t* src = get_block_address(head_);
    std::memcpy(buffer, src, block_size_); // Copy the block data

    head_ = (head_ + 1) % block_count_; // Move head circularly
    current_size_--;

    // std::cout << "CircularFixedSizeQueue: Got block from index " << (head_ - 1 + block_count_) % block_count_ << std::endl; // Use logging
    cv_write_.notify_one(); // Notify one waiting writer that space is available
    return true;
}

std::optional<std::vector<uint8_t>> CircularFixedSizeQueue::Get() {
    std::lock_guard<std::mutex> lock(mutex_); // Thread safe Get (for check and copy)
    if (IsEmpty()) {
        // std::cout << "CircularFixedSizeQueue: Queue is empty, cannot Get." << std::endl; // Use logging
        return std::nullopt;
    }

    std::vector<uint8_t> block_data(block_size_);
    const uint8_t* src = get_block_address(head_);
    std::memcpy(block_data.data(), src, block_size_);

    head_ = (head_ + 1) % block_count_;
    current_size_--;

    // std::cout << "CircularFixedSizeQueue: Got block from index " << (head_ - 1 + block_count_) % block_count_ << " (vector)." << std::endl; // Use logging
    cv_write_.notify_one();
    return block_data;
}


bool CircularFixedSizeQueue::Peek(uint8_t* buffer, size_t buffer_size) const {
     if (buffer == nullptr || buffer_size < block_size_) {
        // std::cerr << "CircularFixedSizeQueue: Peek failed. Invalid buffer or size too small." << std::endl; // Use logging
        return false;
    }
    std::lock_guard<std::mutex> lock(mutex_); // Thread safe Peek

    if (IsEmpty()) {
        // std::cout << "CircularFixedSizeQueue: Queue is empty, cannot Peek." << std::endl; // Use logging
        return false;
    }

    const uint8_t* src = get_block_address(head_);
    std::memcpy(buffer, src, block_size_); // Copy the block data

    // Head and size are not changed by Peek
    // std::cout << "CircularFixedSizeQueue: Peeked block at index " << head_ << std::endl; // Use logging
    return true;
}

std::optional<std::vector<uint8_t>> CircularFixedSizeQueue::Peek() const {
     std::lock_guard<std::mutex> lock(mutex_); // Thread safe Peek (for check and copy)
    if (IsEmpty()) {
        // std::cout << "CircularFixedSizeQueue: Queue is empty, cannot Peek." << std::endl; // Use logging
        return std::nullopt;
    }

    std::vector<uint8_t> block_data(block_size_);
    const uint8_t* src = get_block_address(head_);
    std::memcpy(block_data.data(), src, block_size_);

    // std::cout << "CircularFixedSizeQueue: Peeked block at index " << head_ << " (vector)." << std::endl; // Use logging
    return block_data;
}


// --- Data Access Functions (Blocking) ---
bool CircularFixedSizeQueue::GetBlocking(uint8_t* buffer, size_t buffer_size, long timeout_ms) {
     if (buffer == nullptr || buffer_size < block_size_) {
        // std::cerr << "CircularFixedSizeQueue: GetBlocking failed. Invalid buffer or size too small." << std::endl; // Use logging
        return false;
    }
    std::unique_lock<std::mutex> lock(mutex_); // Use unique_lock for condition variables

    if (IsEmpty()) {
        if (timeout_ms == 0) { // Non-blocking mode
             // std::cout << "CircularFixedSizeQueue: GetBlocking (non-blocking) empty." << std::endl; // Use logging
            return false;
        } else if (timeout_ms > 0) { // Timed wait
            auto duration = std::chrono::milliseconds(timeout_ms);
            if (!cv_read_.wait_for(lock, duration, [&]{ return !IsEmpty(); })) {
                 // std::cout << "CircularFixedSizeQueue: GetBlocking timed out." << std::endl; // Use logging
                 return false; // Timeout
            }
        } else { // Infinite wait
             // std::cout << "CircularFixedSizeQueue: GetBlocking waiting indefinitely." << std::endl; // Use logging
            cv_read_.wait(lock, [&]{ return !IsEmpty(); }); // Wait until not empty
        }
         // If we reached here, the queue is not empty (or we woke up spuriously and checked again)
         if (IsEmpty()) {
             // This can happen after a spurious wakeup or if another thread got the data first after notification
             // std::cout << "CircularFixedSizeQueue: GetBlocking woke up but still empty." << std::endl; // Use logging
             return false; // Still empty
         }
    }

    // Now get the block
    uint8_t* dest = buffer;
    const uint8_t* src = get_block_address(head_);
    std::memcpy(dest, src, block_size_);

    head_ = (head_ + 1) % block_count_;
    current_size_--;

    // std::cout << "CircularFixedSizeQueue: GetBlocking got block." << std::endl; // Use logging
    cv_write_.notify_one(); // Notify one waiting writer that space is available
    return true;
}

std::optional<std::vector<uint8_t>> CircularFixedSizeQueue::GetBlocking(long timeout_ms) {
    std::unique_lock<std::mutex> lock(mutex_); // Use unique_lock for condition variables

    if (IsEmpty()) {
        if (timeout_ms == 0) {
            // std::cout << "CircularFixedSizeQueue: GetBlocking (non-blocking) empty (vector)." << std::endl; // Use logging
            return std::nullopt;
        } else if (timeout_ms > 0) {
            auto duration = std::chrono::milliseconds(timeout_ms);
            if (!cv_read_.wait_for(lock, duration, [&]{ return !IsEmpty(); })) {
                 // std::cout << "CircularFixedSizeQueue: GetBlocking timed out (vector)." << std::endl; // Use logging
                 return std::nullopt; // Timeout
            }
        } else {
             // std::cout << "CircularFixedSizeQueue: GetBlocking waiting indefinitely (vector)." << std::endl; // Use logging
            cv_read_.wait(lock, [&]{ return !IsEmpty(); });
        }
         if (IsEmpty()) {
             // std::cout << "CircularFixedSizeQueue: GetBlocking woke up but still empty (vector)." << std::endl; // Use logging
             return std::nullopt;
         }
    }

    std::vector<uint8_t> block_data(block_size_);
    const uint8_t* src = get_block_address(head_);
    std::memcpy(block_data.data(), src, block_size_);

    head_ = (head_ + 1) % block_count_;
    current_size_--;

    // std::cout << "CircularFixedSizeQueue: GetBlocking got block (vector)." << std::endl; // Use logging
    cv_write_.notify_one();
    return block_data;
}

bool CircularFixedSizeQueue::PutBlocking(const uint8_t* data, size_t data_size, long timeout_ms) {
     if (data == nullptr || data_size != block_size_) {
        // std::cerr << "CircularFixedSizeQueue: PutBlocking failed. Invalid data or size mismatch." << std::endl; // Use logging
        return false;
    }
    std::unique_lock<std::mutex> lock(mutex_); // Use unique_lock for condition variables

    if (IsFull()) {
        if (timeout_ms == 0) { // Non-blocking mode
             // std::cout << "CircularFixedSizeQueue: PutBlocking (non-blocking) full." << std::endl; // Use logging
            return false;
        } else if (timeout_ms > 0) { // Timed wait
            auto duration = std::chrono::milliseconds(timeout_ms);
            if (!cv_write_.wait_for(lock, duration, [&]{ return !IsFull(); })) {
                 // std::cout << "CircularFixedSizeQueue: PutBlocking timed out." << std::endl; // Use logging
                 return false; // Timeout
            }
        } else { // Infinite wait
             // std::cout << "CircularFixedSizeQueue: PutBlocking waiting indefinitely." << std::endl; // Use logging
            cv_write_.wait(lock, [&]{ return !IsFull(); }); // Wait until not full
        }
        if (IsFull()) {
            // std::cout << "CircularFixedSizeQueue: PutBlocking woke up but still full." << std::endl; // Use logging
            return false;
        }
    }

    uint8_t* dest = get_block_address(tail_);
    std::memcpy(dest, data, block_size_);

    tail_ = (tail_ + 1) % block_count_;
    current_size_++;

    // std::cout << "CircularFixedSizeQueue: PutBlocking put block." << std::endl; // Use logging
    cv_read_.notify_one(); // Notify one waiting reader
    return true;
}

bool CircularFixedSizeQueue::PutBlocking(const std::vector<uint8_t>& data, long timeout_ms) {
    return PutBlocking(data.data(), data.size(), timeout_ms);
}


// --- Status Functions ---
bool CircularFixedSizeQueue::IsEmpty() const {
    // Lock needed as current_size_ is shared mutable state
    std::lock_guard<std::mutex> lock(mutex_);
    return current_size_ == 0; // Or head_ == tail_ and current_size_ == 0;
}

bool CircularFixedSizeQueue::IsFull() const {
    // Lock needed as current_size_ is shared mutable state
    std::lock_guard<std::mutex> lock(mutex_);
     return current_size_ == block_count_; // Or head_ == tail_ and current_size_ == block_count_;
}

size_t CircularFixedSizeQueue::Size() const {
    // Lock needed as current_size_ is shared mutable state
    std::lock_guard<std::mutex> lock(mutex_);
    return current_size_;
}

} // namespace Memory
} // namespace LIB_LSX