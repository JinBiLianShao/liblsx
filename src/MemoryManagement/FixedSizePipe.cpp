//
// Created by admin on 2025/5/8.
//
#include "FixedSizePipe.h"

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
uint8_t* FixedSizePipe::get_block_address(size_t index) {
     // No lock here, assumes caller holds the lock
    if (index >= block_count_) {
         throw std::out_of_range("FixedSizePipe: Block index out of range");
    }
    return buffer_.data() + index * block_size_;
}

const uint8_t* FixedSizePipe::get_block_address(size_t index) const {
    // No lock here, assumes caller holds the lock
    if (index >= block_count_) {
         throw std::out_of_range("FixedSizePipe: Block index out of range");
    }
    return buffer_.data() + index * block_size_;
}


FixedSizePipe::FixedSizePipe(size_t block_size, size_t block_count)
    : block_size_(block_size), block_count_(block_count)
{
    if (block_size == 0 || block_count == 0) {
        throw std::invalid_argument("FixedSizePipe: block_size and block_count must be greater than 0");
    }
    try {
        // Lock during construction if shared state could be accessed before constructor finishes
        buffer_.resize(block_size_ * block_count_);
        // std::cout << "FixedSizePipe: Created with block_size=" << block_size_
        //           << ", block_count=" << block_count_
        //           << ", total_size=" << buffer_.size() << std::endl; // Use logging
    } catch (const std::bad_alloc& e) {
        std::cerr << "FixedSizePipe: Failed to allocate buffer: " << e.what() << std::endl;
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

void FixedSizePipe::Clear() {
    std::lock_guard<std::mutex> lock(mutex_); // Thread safe clear
    head_ = 0;
    tail_ = 0;
    current_size_ = 0;
    // Data is not zeroed or erased, just logically reset pointers
    // std::cout << "FixedSizePipe: Cleared." << std::endl; // Use logging
    cv_write_.notify_all(); // Notify potential waiting writers
    cv_read_.notify_all(); // Notify potential waiting readers (they will read 0 blocks)
}

bool FixedSizePipe::Write(const uint8_t* data, size_t data_size) {
    if (data == nullptr || data_size != block_size_) {
        // std::cerr << "FixedSizePipe: Write failed. Invalid data or size mismatch." << std::endl; // Use logging
        return false;
    }
    std::lock_guard<std::mutex> lock(mutex_); // Thread safe Write

    if (IsFull()) {
        // std::cout << "FixedSizePipe: Pipe is full, cannot Write." << std::endl; // Use logging
        return false;
    }

    uint8_t* dest = get_block_address(tail_);
    std::memcpy(dest, data, block_size_); // Copy the block data

    tail_ = (tail_ + 1) % block_count_; // Move tail circularly
    current_size_++;

    // std::cout << "FixedSizePipe: Wrote block at index " << (tail_ - 1 + block_count_) % block_count_ << std::endl; // Use logging
    cv_read_.notify_one(); // Notify one waiting reader that data is available
    return true;
}

bool FixedSizePipe::Write(const std::vector<uint8_t>& data) {
    return Write(data.data(), data.size());
}


bool FixedSizePipe::Read(uint8_t* buffer, size_t buffer_size) {
    if (buffer == nullptr || buffer_size < block_size_) {
        // std::cerr << "FixedSizePipe: Read failed. Invalid buffer or size too small." << std::endl; // Use logging
        return false;
    }
    std::lock_guard<std::mutex> lock(mutex_); // Thread safe Read

    if (IsEmpty()) {
        // std::cout << "FixedSizePipe: Pipe is empty, cannot Read." << std::endl; // Use logging
        return false;
    }

    const uint8_t* src = get_block_address(head_);
    std::memcpy(buffer, src, block_size_); // Copy the block data

    head_ = (head_ + 1) % block_count_; // Move head circularly
    current_size_--;

    // std::cout << "FixedSizePipe: Read block from index " << (head_ - 1 + block_count_) % block_count_ << std::endl; // Use logging
    cv_write_.notify_one(); // Notify one waiting writer that space is available
    return true;
}

std::optional<std::vector<uint8_t>> FixedSizePipe::Read() {
    std::lock_guard<std::mutex> lock(mutex_); // Thread safe Read (for check and copy)
    if (IsEmpty()) {
        // std::cout << "FixedSizePipe: Pipe is empty, cannot Read." << std::endl; // Use logging
        return std::nullopt;
    }

    std::vector<uint8_t> block_data(block_size_);
    const uint8_t* src = get_block_address(head_);
    std::memcpy(block_data.data(), src, block_size_);

    head_ = (head_ + 1) % block_count_;
    current_size_--;

    // std::cout << "FixedSizePipe: Read block from index " << (head_ - 1 + block_count_) % block_count_ << " (vector)." << std::endl; // Use logging
    cv_write_.notify_one();
    return block_data;
}


bool FixedSizePipe::Peek(uint8_t* buffer, size_t buffer_size) const {
     if (buffer == nullptr || buffer_size < block_size_) {
        // std::cerr << "FixedSizePipe: Peek failed. Invalid buffer or size too small." << std::endl; // Use logging
        return false;
    }
    std::lock_guard<std::mutex> lock(mutex_); // Thread safe Peek

    if (IsEmpty()) {
        // std::cout << "FixedSizePipe: Pipe is empty, cannot Peek." << std::endl; // Use logging
        return false;
    }

    const uint8_t* src = get_block_address(head_);
    std::memcpy(buffer, src, block_size_); // Copy the block data

    // Head and size are not changed by Peek
    // std::cout << "FixedSizePipe: Peeked block at index " << head_ << std::endl; // Use logging
    return true;
}

std::optional<std::vector<uint8_t>> FixedSizePipe::Peek() const {
    std::lock_guard<std::mutex> lock(mutex_); // Thread safe Peek (for check and copy)
    if (IsEmpty()) {
        // std::cout << "FixedSizePipe: Pipe is empty, cannot Peek." << std::endl; // Use logging
        return std::nullopt;
    }

    std::vector<uint8_t> block_data(block_size_);
    const uint8_t* src = get_block_address(head_);
    std::memcpy(block_data.data(), src, block_size_);

    // std::cout << "FixedSizePipe: Peeked block at index " << head_ << " (vector)." << std::endl; // Use logging
    return block_data;
}


// --- Data Access Functions (Blocking) ---
bool FixedSizePipe::ReadBlocking(uint8_t* buffer, size_t buffer_size, long timeout_ms) {
     if (buffer == nullptr || buffer_size < block_size_) {
        // std::cerr << "FixedSizePipe: ReadBlocking failed. Invalid buffer or size mismatch." << std::endl; // Use logging
        return false;
    }
    std::unique_lock<std::mutex> lock(mutex_); // Use unique_lock for condition variables

    if (IsEmpty()) {
        if (timeout_ms == 0) { // Non-blocking mode
             // std::cout << "FixedSizePipe: ReadBlocking (non-blocking) empty." << std::endl; // Use logging
            return false;
        } else if (timeout_ms > 0) { // Timed wait
            auto duration = std::chrono::milliseconds(timeout_ms);
            if (!cv_read_.wait_for(lock, duration, [&]{ return !IsEmpty(); })) {
                 // std::cout << "FixedSizePipe: ReadBlocking timed out." << std::endl; // Use logging
                 return false; // Timeout
            }
        } else { // Infinite wait
             // std::cout << "FixedSizePipe: ReadBlocking waiting indefinitely." << std::endl; // Use logging
            cv_read_.wait(lock, [&]{ return !IsEmpty(); }); // Wait until not empty
        }
         // If we reached here, the queue is not empty (or we woke up spuriously and checked again)
         if (IsEmpty()) {
             // This can happen after a spurious wakeup or if another thread got the data first after notification
             // In a robust system, you might need to re-wait or return a status indicating no data *was* available *at this moment*
             // For simplicity, we return false if still empty after wait.
             // std::cout << "FixedSizePipe: ReadBlocking woke up but still empty." << std::endl; // Use logging
             return false;
         }
    }

    // Now read the block (similar to non-blocking Read)
    uint8_t* dest = buffer; // Assuming buffer_size is >= block_size_
    const uint8_t* src = get_block_address(head_);
    std::memcpy(dest, src, block_size_);

    head_ = (head_ + 1) % block_count_;
    current_size_--;

    // std::cout << "FixedSizePipe: ReadBlocking got block." << std::endl; // Use logging
    cv_write_.notify_one(); // Notify one waiting writer that space is available
    return true;
}

std::optional<std::vector<uint8_t>> FixedSizePipe::ReadBlocking(long timeout_ms) {
    std::unique_lock<std::mutex> lock(mutex_); // Use unique_lock for condition variables

    if (IsEmpty()) {
        if (timeout_ms == 0) {
            // std::cout << "FixedSizePipe: ReadBlocking (non-blocking) empty (vector)." << std::endl; // Use logging
            return std::nullopt;
        } else if (timeout_ms > 0) {
            auto duration = std::chrono::milliseconds(timeout_ms);
            if (!cv_read_.wait_for(lock, duration, [&]{ return !IsEmpty(); })) {
                 // std::cout << "FixedSizePipe: ReadBlocking timed out (vector)." << std::endl; // Use logging
                 return std::nullopt; // Timeout
            }
        } else {
             // std::cout << "FixedSizePipe: ReadBlocking waiting indefinitely (vector)." << std::endl; // Use logging
            cv_read_.wait(lock, [&]{ return !IsEmpty(); });
        }
         if (IsEmpty()) {
             // std::cout << "FixedSizePipe: ReadBlocking woke up but still empty (vector)." << std::endl; // Use logging
             return std::nullopt;
         }
    }

    std::vector<uint8_t> block_data(block_size_);
    const uint8_t* src = get_block_address(head_);
    std::memcpy(block_data.data(), src, block_size_);

    head_ = (head_ + 1) % block_count_;
    current_size_--;

    // std::cout << "FixedSizePipe: ReadBlocking got block (vector)." << std::endl; // Use logging
    cv_write_.notify_one();
    return block_data;
}

bool FixedSizePipe::WriteBlocking(const uint8_t* data, size_t data_size, long timeout_ms) {
     if (data == nullptr || data_size != block_size_) {
        // std::cerr << "FixedSizePipe: WriteBlocking failed. Invalid data or size mismatch." << std::endl; // Use logging
        return false;
    }
    std::unique_lock<std::mutex> lock(mutex_); // Use unique_lock for condition variables

    if (IsFull()) {
        if (timeout_ms == 0) { // Non-blocking mode
             // std::cout << "FixedSizePipe: WriteBlocking (non-blocking) full." << std::endl; // Use logging
            return false;
        } else if (timeout_ms > 0) { // Timed wait
            auto duration = std::chrono::milliseconds(timeout_ms);
            if (!cv_write_.wait_for(lock, duration, [&]{ return !IsFull(); })) {
                 // std::cout << "FixedSizePipe: WriteBlocking timed out." << std::endl; // Use logging
                 return false; // Timeout
            }
        } else { // Infinite wait
             // std::cout << "FixedSizePipe: WriteBlocking waiting indefinitely." << std::endl; // Use logging
            cv_write_.wait(lock, [&]{ return !IsFull(); }); // Wait until not full
        }
        if (IsFull()) {
            // std::cout << "FixedSizePipe: WriteBlocking woke up but still full." << std::endl; // Use logging
            return false;
        }
    }

    uint8_t* dest = get_block_address(tail_);
    std::memcpy(dest, data, block_size_);

    tail_ = (tail_ + 1) % block_count_;
    current_size_++;

    // std::cout << "FixedSizePipe: WriteBlocking put block." << std::endl; // Use logging
    cv_read_.notify_one(); // Notify one waiting reader
    return true;
}

bool FixedSizePipe::WriteBlocking(const std::vector<uint8_t>& data, long timeout_ms) {
    return WriteBlocking(data.data(), data.size(), timeout_ms);
}


// --- Status Functions ---
bool FixedSizePipe::IsEmpty() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return current_size_ == 0;
}

bool FixedSizePipe::IsFull() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return current_size_ == block_count_;
}

size_t FixedSizePipe::Size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return current_size_;
}

} // namespace Memory
} // namespace LIB_LSX