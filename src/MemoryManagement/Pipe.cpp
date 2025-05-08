#include "Pipe.h"

#include <algorithm> // For std::min
#include <mutex>   // For std::mutex, std::lock_guard, std::unique_lock
#include <condition_variable> // For std::condition_variable
#include <chrono> // For std::chrono::milliseconds
// #include <iostream>  // For example output - prefer logging

namespace LIB_LSX {
namespace Memory {

Pipe::Pipe() {
    // Constructor implementation (if needed beyond default)
    // std::cout << "Pipe: Created." << std::endl; // Use logging
}

Pipe::~Pipe() {
    // Destructor implementation (if needed beyond default)
    // std::cout << "Pipe: Destroyed." << std::endl; // Use logging
}

void Pipe::Clear() {
    std::lock_guard<std::mutex> lock(mutex_); // Thread safe clear
    byte_stream_.clear(); // std::deque::clear()
    // std::cout << "Pipe: Cleared." << std::endl; // Use logging
    // cv_write_.notify_all(); // Notify potential waiting writers (if bounded pipe)
    cv_read_.notify_all(); // Notify potential waiting readers (they will read 0 bytes)
}


size_t Pipe::Write(const uint8_t* data, size_t size) {
    if (data == nullptr || size == 0) {
        return 0;
    }
    std::lock_guard<std::mutex> lock(mutex_); // Thread safe write

    // Optional: check and wait if pipe is full (for bounded pipe)
    // if (byte_stream_.size() + size > capacity_) { /* handle full */ }

    size_t bytes_to_write = size; // In unbounded pipe, write all

    for (size_t i = 0; i < bytes_to_write; ++i) {
        byte_stream_.push_back(data[i]);
    }
    // std::cout << "Pipe: Wrote " << bytes_to_write << " bytes." << std::endl; // Use logging

    cv_read_.notify_all(); // Notify readers (all waiting readers might be able to read now)

    return bytes_to_write;
}

size_t Pipe::Write(const std::vector<uint8_t>& data) {
    return Write(data.data(), data.size());
}


size_t Pipe::Read(uint8_t* buffer, size_t size) {
    if (buffer == nullptr || size == 0) {
        return 0;
    }
    std::lock_guard<std::mutex> lock(mutex_); // Thread safe read

    // Optional: check and wait if pipe is empty (for blocking read, handled in ReadBlocking)

    size_t bytes_to_read = std::min(size, byte_stream_.size());

    for (size_t i = 0; i < bytes_to_read; ++i) {
        buffer[i] = byte_stream_.front();
        byte_stream_.pop_front();
    }
    // std::cout << "Pipe: Read " << bytes_to_read << " bytes." << std::endl; // Use logging

    // cv_write_.notify_all(); // Notify writers (if bounded pipe and space was freed)

    return bytes_to_read;
}

std::vector<uint8_t> Pipe::Read(size_t size) {
    std::vector<uint8_t> buffer(size);
    size_t bytes_read = Read(buffer.data(), size);
    buffer.resize(bytes_read); // Resize vector to actual bytes read
    return buffer;
}

size_t Pipe::Peek(uint8_t* buffer, size_t size) const {
    if (buffer == nullptr || size == 0) {
        return 0;
    }
    std::lock_guard<std::mutex> lock(mutex_); // Thread safe peek

    size_t bytes_to_peek = std::min(size, byte_stream_.size());

    for (size_t i = 0; i < bytes_to_peek; ++i) {
        buffer[i] = byte_stream_[i]; // Access directly
    }
    // std::cout << "Pipe: Peeked " << bytes_to_peek << " bytes." << std::endl; // Use logging
    return bytes_to_peek;
}

std::vector<uint8_t> Pipe::Peek(size_t size) const {
    std::vector<uint8_t> buffer(size);
    size_t bytes_peeked = Peek(buffer.data(), size);
    buffer.resize(bytes_peeked);
    return buffer;
}


// --- Data Access Functions (Blocking) ---
size_t Pipe::ReadBlocking(uint8_t* buffer, size_t size, long timeout_ms) {
    if (buffer == nullptr || size == 0) {
        return 0;
    }
    std::unique_lock<std::mutex> lock(mutex_); // Use unique_lock for condition variables

    if (byte_stream_.empty()) {
        if (timeout_ms == 0) { // Non-blocking mode
             // std::cout << "Pipe: ReadBlocking (non-blocking) empty." << std::endl; // Use logging
            return 0;
        } else if (timeout_ms > 0) { // Timed wait
            auto duration = std::chrono::milliseconds(timeout_ms);
            // wait_for returns false if the timeout elapsed without notification
            if (!cv_read_.wait_for(lock, duration, [&]{ return !byte_stream_.empty(); })) {
                 // std::cout << "Pipe: ReadBlocking timed out." << std::endl; // Use logging
                 return 0; // Timeout
            }
        } else { // Infinite wait
             // std::cout << "Pipe: ReadBlocking waiting indefinitely." << std::endl; // Use logging
            cv_read_.wait(lock, [&]{ return !byte_stream_.empty(); }); // Wait until not empty
        }
         // If we reached here, the queue is not empty (or we woke up spuriously and checked again)
         // Check again if queue is still empty after wait.
         if (byte_stream_.empty()) {
             // std::cout << "Pipe: ReadBlocking woke up but still empty." << std::endl; // Use logging
             return 0; // Still empty
         }
    }

    // Now read, similar to non-blocking Read
    size_t bytes_to_read = std::min(size, byte_stream_.size());
    for (size_t i = 0; i < bytes_to_read; ++i) {
        buffer[i] = byte_stream_.front();
        byte_stream_.pop_front();
    }
    // std::cout << "Pipe: ReadBlocking got " << bytes_to_read << " bytes." << std::endl; // Use logging
    // cv_write_.notify_all(); // Notify writers (if bounded pipe)
    return bytes_to_read;
}

std::vector<uint8_t> Pipe::ReadBlocking(size_t size, long timeout_ms) {
    std::vector<uint8_t> buffer(size);
    size_t bytes_read = ReadBlocking(buffer.data(), size, timeout_ms);
    buffer.resize(bytes_read);
    return buffer;
}


size_t Pipe::WriteBlocking(const uint8_t* data, size_t size, long timeout_ms) {
    if (data == nullptr || size == 0) {
        return 0;
    }
    std::unique_lock<std::mutex> lock(mutex_); // Use unique_lock for condition variables

    // Optional: check and wait if pipe is full (for bounded pipe)
    // if (byte_stream_.size() + size > capacity_) {
    //     if (timeout_ms == 0) return 0;
    //     auto duration = std::chrono::milliseconds(timeout_ms);
    //     if (!cv_write_.wait_for(lock, duration, [&]{ return byte_stream_.size() + size <= capacity_; })) {
    //          return 0; // Timeout or failed to free enough space
    //     }
    //      if (byte_stream_.size() + size > capacity_) return 0; // Still full after wait
    // }

    size_t bytes_to_write = size; // In unbounded pipe, write all

    for (size_t i = 0; i < bytes_to_write; ++i) {
        byte_stream_.push_back(data[i]);
    }
    // std::cout << "Pipe: WriteBlocking put " << bytes_to_write << " bytes." << std::endl; // Use logging
    cv_read_.notify_all(); // Notify readers

    return bytes_to_write;
}

size_t Pipe::WriteBlocking(const std::vector<uint8_t>& data, long timeout_ms) {
    return WriteBlocking(data.data(), data.size(), timeout_ms);
}


// --- Status Functions ---
bool Pipe::IsEmpty() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return byte_stream_.empty();
}

size_t Pipe::Size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return byte_stream_.size();
}

// bool Pipe::IsFull() const { /* ... */ } // If bounded
// size_t Pipe::Capacity() const { /* ... */ } // If bounded

} // namespace Memory
} // namespace LIB_LSX