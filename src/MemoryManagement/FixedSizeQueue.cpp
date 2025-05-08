#include "FixedSizeQueue.h"

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
uint8_t* FixedSizeQueue::get_block_address(size_t index) {
    // No lock here, assumes caller holds the lock
    if (index >= block_count_) {
         throw std::out_of_range("FixedSizeQueue: Block index out of range");
    }
    return buffer_.data() + index * block_size_;
}

const uint8_t* FixedSizeQueue::get_block_address(size_t index) const {
    // No lock here, assumes caller holds the lock
    if (index >= block_count_) {
         throw std::out_of_range("FixedSizeQueue: Block index out of range");
    }
    return buffer_.data() + index * block_size_;
}


FixedSizeQueue::FixedSizeQueue(size_t block_size, size_t block_count)
    : block_size_(block_size), block_count_(block_count)
{
    if (block_size == 0 || block_count == 0) {
        throw std::invalid_argument("FixedSizeQueue: block_size and block_count must be greater than 0");
    }
    try {
        // Lock during construction if shared state could be accessed before constructor finishes
        buffer_.resize(block_size_ * block_count_);
        // std::cout << "FixedSizeQueue: Created with block_size=" << block_size_
        //           << ", block_count=" << block_count_
        //           << ", total_size=" << buffer_.size() << std::endl; // Use logging
    } catch (const std::bad_alloc& e) {
        std::cerr << "FixedSizeQueue: Failed to allocate buffer: " << e.what() << std::endl;
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

void FixedSizeQueue::Clear() {
    std::lock_guard<std::mutex> lock(mutex_); // Thread safe clear
    head_ = 0;
    tail_ = 0;
    current_size_ = 0;
    // Data is not zeroed or erased, just logically reset pointers
    // std::cout << "FixedSizeQueue: Cleared." << std::endl; // Use logging
    cv_write_.notify_all(); // Notify potential waiting writers that space is available
    cv_read_.notify_all(); // Notify potential waiting readers (they will read 0 blocks)
}

bool FixedSizeQueue::Put(const uint8_t* data, size_t data_size) {
    if (data == nullptr || data_size != block_size_) {
        // std::cerr << "FixedSizeQueue: Put failed. Invalid data or size mismatch." << std::endl; // Use logging
        return false;
    }
    std::lock_guard<std::mutex> lock(mutex_); // Thread safe Put

    if (IsFull()) {
        // std::cout << "FixedSizeQueue: Queue is full, cannot Put." << std::endl; // Use logging
        return false;
    }

    uint8_t* dest = get_block_address(tail_);
    std::memcpy(dest, data, block_size_); // Copy the block data

    tail_ = (tail_ + 1); // Move tail linearly (until wrap in Get)
    current_size_++;

    // std::cout << "FixedSizeQueue: Put block at index " << tail_ - 1 << std::endl; // Use logging
    cv_read_.notify_one(); // Notify one waiting reader that data is available
    return true;
}

bool FixedSizeQueue::Put(const std::vector<uint8_t>& data) {
    return Put(data.data(), data.size());
}


bool FixedSizeQueue::Get(uint8_t* buffer, size_t buffer_size) {
    if (buffer == nullptr || buffer_size < block_size_) {
        // std::cerr << "FixedSizeQueue: Get failed. Invalid buffer or size too small." << std::endl; // Use logging
        return false;
    }
    std::lock_guard<std::mutex> lock(mutex_); // Thread safe Get

    if (IsEmpty()) {
        // std::cout << "FixedSizeQueue: Queue is empty, cannot Get." << std::endl; // Use logging
        return false;
    }

    const uint8_t* src = get_block_address(head_ % block_count_); // Use index modulo block_count
    std::memcpy(buffer, src, block_size_); // Copy the block data

    head_ = (head_ + 1); // Move head linearly
    current_size_--;

    // std::cout << "FixedSizeQueue: Got block from index " << (head_ - 1) % block_count_ << std::endl; // Use logging
    cv_write_.notify_one(); // Notify one waiting writer that space is available
    return true;
}

std::optional<std::vector<uint8_t>> FixedSizeQueue::Get() {
    std::lock_guard<std::mutex> lock(mutex_); // Thread safe Get (for check and copy)
    if (IsEmpty()) {
        // std::cout << "FixedSizeQueue: Queue is empty, cannot Get." << std::endl; // Use logging
        return std::nullopt;
    }

    std::vector<uint8_t> block_data(block_size_);
    const uint8_t* src = get_block_address(head_ % block_count_);
    std::memcpy(block_data.data(), src, block_size_);

    head_ = (head_ + 1);
    current_size_--;

    // std::cout << "FixedSizeQueue: Got block from index " << (head_ - 1) % block_count_ << " (vector)." << std::endl; // Use logging
    cv_write_.notify_one();
    return block_data;
}


bool FixedSizeQueue::Peek(uint8_t* buffer, size_t buffer_size) const {
     if (buffer == nullptr || buffer_size < block_size_) {
        // std::cerr << "FixedSizeQueue: Peek failed. Invalid buffer or size too small." << std::endl; // Use logging
        return false;
    }
    std::lock_guard<std::mutex> lock(mutex_); // Thread safe Peek

    if (IsEmpty()) {
        // std::cout << "FixedSizeQueue: Queue is empty, cannot Peek." << std::endl; // Use logging
        return false;
    }

    const uint8_t* src = get_block_address(head_ % block_count_);
    std::memcpy(buffer, src, block_size_); // Copy the block data

    // Head and size are not changed by Peek
    // std::cout << "FixedSizeQueue: Peeked block at index " << head_ % block_count_ << std::endl; // Use logging
    return true;
}

std::optional<std::vector<uint8_t>> FixedSizeQueue::Peek() const {
     std::lock_guard<std::mutex> lock(mutex_); // Thread safe Peek (for check and copy)
    if (IsEmpty()) {
        // std::cout << "FixedSizeQueue: Queue is empty, cannot Peek." << std::endl; // Use logging
        return std::nullopt;
    }

    std::vector<uint8_t> block_data(block_size_);
    const uint8_t* src = get_block_address(head_ % block_count_);
    std::memcpy(block_data.data(), src, block_size_);

    // std::cout << "FixedSizeQueue: Peeked block at index " << head_ % block_count_ << " (vector)." << std::endl; // Use logging
    return block_data;
}


// --- Data Access Functions (Blocking) ---
bool FixedSizeQueue::GetBlocking(uint8_t* buffer, size_t buffer_size, long timeout_ms) {
     if (buffer == nullptr || buffer_size < block_size_) {
        // std::cerr << "FixedSizeQueue: GetBlocking failed. Invalid buffer or size too small." << std::endl; // Use logging
        return false;
    }
    std::unique_lock<std::mutex> lock(mutex_); // Use unique_lock for condition variables

    if (IsEmpty()) {
        if (timeout_ms == 0) { // Non-blocking mode
             // std::cout << "FixedSizeQueue: GetBlocking (non-blocking) empty." << std::endl; // Use logging
            return false;
        } else if (timeout_ms > 0) { // Timed wait
            auto duration = std::chrono::milliseconds(timeout_ms);
            if (!cv_read_.wait_for(lock, duration, [&]{ return !IsEmpty(); })) {
                 // std::cout << "FixedSizeQueue: GetBlocking timed out." << std::endl; // Use logging
                 return false; // Timeout
            }
        } else { // Infinite wait
             // std::cout << "FixedSizeQueue: GetBlocking waiting indefinitely." << std::endl; // Use logging
            cv_read_.wait(lock, [&]{ return !IsEmpty(); }); // Wait until not empty
        }
         if (IsEmpty()) {
             // std::cout << "FixedSizeQueue: GetBlocking woke up but still empty." << std::endl; // Use logging
             return false;
         }
    }

    // Now get the block
    uint8_t* dest = buffer; // Assuming buffer_size is >= block_size_
    const uint8_t* src = get_block_address(head_ % block_count_);
    std::memcpy(dest, src, block_size_);

    head_ = (head_ + 1);
    current_size_--;

    // std::cout << "FixedSizeQueue: GetBlocking got block." << std::endl; // Use logging
    cv_write_.notify_one(); // Notify one waiting writer that space is available
    return true;
}

std::optional<std::vector<uint8_t>> FixedSizeQueue::GetBlocking(long timeout_ms) {
    std::unique_lock<std::mutex> lock(mutex_); // Use unique_lock for condition variables

    if (IsEmpty()) {
        if (timeout_ms == 0) {
            // std::cout << "FixedSizeQueue: GetBlocking (non-blocking) empty (vector)." << std::endl; // Use logging
            return std::nullopt;
        } else if (timeout_ms > 0) {
            auto duration = std::chrono::milliseconds(timeout_ms);
            if (!cv_read_.wait_for(lock, duration, [&]{ return !IsEmpty(); })) {
                 // std::cout << "FixedSizeQueue: GetBlocking timed out (vector)." << std::endl; // Use logging
                 return std::nullopt; // Timeout
            }
        } else {
             // std::cout << "FixedSizeQueue: GetBlocking waiting indefinitely (vector)." << std::endl; // Use logging
            cv_read_.wait(lock, [&]{ return !IsEmpty(); });
        }
         if (IsEmpty()) {
             // std::cout << "FixedSizeQueue: GetBlocking woke up but still empty (vector)." << std::endl; // Use logging
             return std::nullopt;
         }
    }

    std::vector<uint8_t> block_data(block_size_);
    const uint8_t* src = get_block_address(head_ % block_count_);
    std::memcpy(block_data.data(), src, block_size_);

    head_ = (head_ + 1);
    current_size_--;

    // std::cout << "FixedSizeQueue: GetBlocking got block (vector)." << std::endl; // Use logging
    cv_write_.notify_one();
    return block_data;
}

bool FixedSizeQueue::PutBlocking(const uint8_t* data, size_t data_size, long timeout_ms) {
     if (data == nullptr || data_size != block_size_) {
        // std::cerr << "FixedSizeQueue: PutBlocking failed. Invalid data or size mismatch." << std::endl; // Use logging
        return false;
    }
    std::unique_lock<std::mutex> lock(mutex_); // Use unique_lock for condition variables

    if (IsFull()) {
        if (timeout_ms == 0) { // Non-blocking mode
             // std::cout << "FixedSizeQueue: PutBlocking (non-blocking) full." << std::endl; // Use logging
            return false;
        } else if (timeout_ms > 0) { // Timed wait
            auto duration = std::chrono::milliseconds(timeout_ms);
            if (!cv_write_.wait_for(lock, duration, [&]{ return !IsFull(); })) {
                 // std::cout << "FixedSizeQueue: PutBlocking timed out." << std::endl; // Use logging
                 return false; // Timeout
            }
        } else { // Infinite wait
             // std::cout << "FixedSizeQueue: PutBlocking waiting indefinitely." << std::endl; // Use logging
            cv_write_.wait(lock, [&]{ return !IsFull(); }); // Wait until not full
        }
        if (IsFull()) {
            // std::cout << "FixedSizeQueue: PutBlocking woke up but still full." << std::endl; // Use logging
            return false;
        }
    }

    uint8_t* dest = get_block_address(tail_ % block_count_);
    std::memcpy(dest, data, block_size_);

    tail_ = (tail_ + 1);
    current_size_++;

    // std::cout << "FixedSizeQueue: PutBlocking put block." << std::endl; // Use logging
    cv_read_.notify_one(); // Notify one waiting reader
    return true;
}

bool FixedSizeQueue::PutBlocking(const std::vector<uint8_t>& data, long timeout_ms) {
    return PutBlocking(data.data(), data.size(), timeout_ms);
}


// --- Status Functions ---
bool FixedSizeQueue::IsEmpty() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return current_size_ == 0;
}

bool FixedSizeQueue::IsFull() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return current_size_ == block_count_;
}

size_t FixedSizeQueue::Size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return current_size_;
}

} // namespace Memory
} // namespace LIB_LSX