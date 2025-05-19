#include "Buffer.h"

#include <cstring>
#include <algorithm>
 #include <iostream>
#include <iostream>
#include "LockGuard.h"
#include "MultiLockGuard.h"


namespace LIB_LSX {
namespace Memory {

Buffer::Buffer() {
    // std::cout << "Buffer: Created empty buffer." << std::endl; // Use logging
}

Buffer::Buffer(size_t size) {
    if (size > 0) {
        try {
            LSX_LIB::LockManager::LockGuard<std::mutex> lock(mutex_);
            data_vector_.resize(size);
            // std::cout << "Buffer: Allocated buffer of size " << size << std::endl; // Use logging
        } catch (const std::bad_alloc& e) {
             std::cerr << "Buffer: Failed to allocate buffer of size " << size << ": " << e.what() << std::endl;
             // data_vector_ will be empty - safe state
             throw; // Re-throw
        }
    } else {
         // std::cout << "Buffer: Created buffer with requested size 0 (empty)." << std::endl; // Use logging
    }
}

bool Buffer::Resize(size_t new_size) {
    try {
        LSX_LIB::LockManager::LockGuard<std::mutex> lock(mutex_);
        data_vector_.resize(new_size);
        // std::cout << "Buffer: Resized buffer to " << new_size << std::endl; // Use logging
        return true;
    } catch (const std::bad_alloc& e) {
        std::cerr << "Buffer: Failed to resize buffer to " << new_size << ": " << e.what() << std::endl;
        return false;
    }
}

void Buffer::Clear() {
    Resize(0); // Resize to empty handles locking internally
    // std::cout << "Buffer: Cleared." << std::endl; // Use logging
}

void Buffer::Fill(uint8_t value) {
     LSX_LIB::LockManager::LockGuard<std::mutex> lock(mutex_);
    if (!data_vector_.empty()) {
        std::fill(data_vector_.begin(), data_vector_.end(), value);
        // std::cout << "Buffer: Filled buffer with value " << (int)value << std::endl; // Use logging
    }
}


uint8_t* Buffer::GetData() {
    // Locking here provides a snapshot of the pointer, but doesn't protect *access* via the pointer
    // concurrent with Resize. The WriteAt/ReadAt methods provide synchronized access.
    // If a user gets the pointer and accesses it without calling WriteAt/ReadAt,
    // they need external synchronization or must ensure no concurrent Resize occurs.
    LSX_LIB::LockManager::LockGuard<std::mutex> lock(mutex_);
    if (data_vector_.empty()) {
        return nullptr;
    }
    return data_vector_.data();
}

const uint8_t* Buffer::GetData() const {
     LSX_LIB::LockManager::LockGuard<std::mutex> lock(mutex_);
    if (data_vector_.empty()) {
        return nullptr;
    }
    return data_vector_.data();
}

size_t Buffer::WriteAt(size_t offset, const uint8_t* data, size_t size) {
     if (data == nullptr || size == 0) {
         return 0;
     }
     LSX_LIB::LockManager::LockGuard<std::mutex> lock(mutex_); // Protect data_vector_ and its data pointer

     if (offset >= data_vector_.size()) {
         // std::cerr << "Buffer: WriteAt failed. Offset " << offset << " is out of bounds (size " << data_vector_.size() << ")." << std::endl; // Use logging
         return 0;
     }

     size_t bytes_to_write = std::min(size, data_vector_.size() - offset);
     if (bytes_to_write > 0) {
         uint8_t* dest = data_vector_.data() + offset;
         std::memcpy(dest, data, bytes_to_write);
        // std::cout << "Buffer: Wrote " << bytes_to_write << " bytes at offset " << offset << "." << std::endl; // Use logging
     }
     return bytes_to_write;
}

size_t Buffer::WriteAt(size_t offset, const std::vector<uint8_t>& data) {
    return WriteAt(offset, data.data(), data.size());
}


size_t Buffer::ReadAt(size_t offset, uint8_t* buffer, size_t size) const {
    if (buffer == nullptr || size == 0) {
        return 0;
    }
    LSX_LIB::LockManager::LockGuard<std::mutex> lock(mutex_); // Protect data_vector_ and its data pointer

    if (offset >= data_vector_.size()) {
        // std::cerr << "Buffer: ReadAt failed. Offset " << offset << " is out of bounds (size " << data_vector_.size() << ")." << std::endl; // Use logging
        return 0;
    }

    size_t bytes_to_read = std::min(size, data_vector_.size() - offset);
     if (bytes_to_read > 0) {
        const uint8_t* src = data_vector_.data() + offset;
        std::memcpy(buffer, src, bytes_to_read);
        // std::cout << "Buffer: Read " << bytes_to_read << " bytes from offset " << offset << "." << std::endl; // Use logging
     }
    return bytes_to_read;
}

std::vector<uint8_t> Buffer::ReadAt(size_t offset, size_t size) const {
    std::vector<uint8_t> buffer(size);
    size_t bytes_read = ReadAt(offset, buffer.data(), size);
    buffer.resize(bytes_read);
    return buffer;
}


size_t Buffer::GetSize() const {
    LSX_LIB::LockManager::LockGuard<std::mutex> lock(mutex_);
    return data_vector_.size();
}

bool Buffer::IsEmpty() const {
    LSX_LIB::LockManager::LockGuard<std::mutex> lock(mutex_);
    return data_vector_.empty();
}

size_t Buffer::Capacity() const {
     LSX_LIB::LockManager::LockGuard<std::mutex> lock(mutex_);
     // For std::vector based buffer, capacity is size unless reserved
     // Returning size is fine here as it represents the usable space managed by this class
     return data_vector_.size();
}


} // namespace Memory
} // namespace LIB_LSX