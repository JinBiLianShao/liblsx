#pragma once
#include "SharedMemory.h"

#include <iostream>
#include <cstring> // For strncpy, memcpy
#include <algorithm> // For std::min
#include <stdexcept> // For exceptions
#include "LockGuard.h"
#include "MultiLockGuard.h"

// Include OS-specific headers here for implementation
#ifdef _WIN32
#include <windows.h>
#else // POSIX
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <unistd.h>
#include <cerrno> // For errno
#endif


namespace LIB_LSX {
namespace Memory {

SharedMemory::SharedMemory() {
    // std::cout << "SharedMemory: Instance created." << std::endl; // Use logging
}

SharedMemory::~SharedMemory() {
    // std::cout << "SharedMemory: Instance destroying." << std::endl; // Use logging
    // Detach if still attached
    Detach();
    // Destroy only if this instance created it and it's not already marked for deletion
    if (is_owner_) {
        Destroy(); // Call Destroy here
    }
}

bool SharedMemory::Create(const std::string& key_or_name, size_t size) {
    LSX_LIB::LockManager::LockGuard<std::mutex> lock(mutex_); // Protect instance state during creation

    if (size == 0) {
        std::cerr << "SharedMemory: Create failed. Size must be greater than 0." << std::endl;
        return false;
    }
    if (IsAttached() || !key_name_.empty()) { // Check if already managing a segment (locked by lock_guard)
         std::cerr << "SharedMemory: Create failed. Already managing a segment ('" << key_name_ << "'). Detach first." << std::endl;
         return false;
    }

    key_name_ = key_or_name;
    size_ = size; // Store requested size

#ifdef _WIN32
    // Windows specific implementation
    LPCSTR win_name = key_or_name.c_str();

    hMapFile_ = CreateFileMapping(
        INVALID_HANDLE_VALUE, // Use paging file
        NULL,                 // Default security
        PAGE_READWRITE,       // Read/write access
        (DWORD)(size >> 32),  // High-order DWORD of size
        (DWORD)(size & 0xFFFFFFFF), // Low-order DWORD of size
        win_name); // Object name

    if (hMapFile_ == NULL) {
        std::cerr << "SharedMemory [Win]: CreateFileMapping failed for '" << key_or_name << "' (" << GetLastError() << ")." << std::endl;
        key_name_ = ""; size_ = 0; // Reset state
        return false;
    }

    // Check if the mapping object already existed (we are the creator if not)
    if (GetLastError() != ERROR_ALREADY_EXISTS) {
         is_owner_ = true;
         // std::cout << "SharedMemory [Win]: Created segment '" << key_or_name << "' size " << size << std::endl; // Use logging
    } else {
         is_owner_ = false; // It existed, we just got a handle to it
         // std::cout << "SharedMemory [Win]: Opened existing segment '" << key_or_name << "' size " << size << std::endl; // Use logging
         // Note: If opening existing, the actual size is determined by the creator.
         // A robust library might check actual size vs requested size here using GetFileSize, etc.
    }

    // Attach/map the view
    lpBaseAddress_ = MapViewOfFile(
        hMapFile_,            // Handle to map object
        FILE_MAP_ALL_ACCESS,  // Read/write permission
        0, 0, size);

    if (lpBaseAddress_ == NULL) {
        std::cerr << "SharedMemory [Win]: MapViewOfFile failed (" << GetLastError() << ")." << std::endl;
        CloseHandle(hMapFile_); hMapFile_ = NULL;
        key_name_ = ""; size_ = 0; is_owner_ = false; // Reset state
        return false;
    }
    // std::cout << "SharedMemory [Win]: Attached to segment at address: " << lpBaseAddress_ << std::endl; // Use logging
    return true;

#else // POSIX specific implementation
    shm_key_ = ftok(key_or_name.c_str(), 'R');
    if (shm_key_ == -1) {
        std::cerr << "SharedMemory [POSIX]: ftok failed for '" << key_or_name << "' (" << errno << ": " << strerror(errno) << ")." << std::endl;
        key_name_ = ""; size_ = 0; // Reset state
        return false;
    }
    // std::cout << "SharedMemory [POSIX]: Generated key: " << shm_key_ << " from '" << key_or_name << "'" << std::endl; // Use logging


    shm_id_ = shmget(shm_key_, size, IPC_CREAT | IPC_EXCL | 0600); // Try creating exclusively

    if (shm_id_ == -1) {
         if (errno == EEXIST) {
             // Creation failed because it exists. Fail the 'Create' call as per its purpose.
             std::cerr << "SharedMemory [POSIX]: Failed to create segment with key " << shm_key_ << " because it already exists." << std::endl;
         } else {
            std::cerr << "SharedMemory [POSIX]: Failed to create segment (" << errno << ": " << strerror(errno) << ")." << std::endl;
         }
         shm_key_ = -1; key_name_ = ""; size_ = 0; // Reset state
         return false; // Creation failed
    } else {
        // Successfully created the segment
        is_owner_ = true;
        // std::cout << "SharedMemory [POSIX]: Created segment ID: " << shm_id_ << " key: " << shm_key_ << " size: " << size << std::endl; // Use logging
    }

    // Attach the shared memory segment
    shm_address_ = shmat(shm_id_, nullptr, 0); // Attach anywhere with default flags
    if (shm_address_ == (void*)-1) {
        std::cerr << "SharedMemory [POSIX]: Failed to attach segment ID " << shm_id_ << " (" << errno << ": " << strerror(errno) << ")." << std::endl;
        // If we created it, clean up the segment ID as we couldn't attach
        shmctl(shm_id_, IPC_RMID, nullptr); // Mark for deletion
        shm_id_ = -1; shm_address_ = nullptr; shm_key_ = -1; key_name_ = ""; size_ = 0; is_owner_ = false; // Reset state
        return false; // Attachment failed
    }

    // std::cout << "SharedMemory [POSIX]: Attached to segment ID " << shm_id_ << " at address: " << shm_address_ << std::endl; // Use logging
    return true;
#endif
}


bool SharedMemory::Open(const std::string& key_or_name, size_t size) {
     LSX_LIB::LockManager::LockGuard<std::mutex> lock(mutex_); // Protect instance state

     if (size == 0) {
         std::cerr << "SharedMemory: Open failed. Size must be greater than 0." << std::endl;
         return false;
     }
      if (IsAttached() || !key_name_.empty()) { // Check if already managing a segment (locked by lock_guard)
         std::cerr << "SharedMemory: Open failed. Already managing a segment ('" << key_name_ << "'). Detach first." << std::endl;
         return false;
      }

     key_name_ = key_or_name;
     size_ = size; // Store requested size

#ifdef _WIN32
     // Windows specific: Use OpenFileMapping to open an existing one
     LPCSTR win_name = key_or_name.c_str();
     hMapFile_ = OpenFileMapping(
        FILE_MAP_ALL_ACCESS, // Read/write access
        FALSE,               // Do not inherit the handle
        win_name); // Object name

     if (hMapFile_ == NULL) {
         std::cerr << "SharedMemory [Win]: OpenFileMapping failed for '" << key_or_name << "' (" << GetLastError() << ")." << std::endl;
         key_name_ = ""; size_ = 0; // Reset state
         return false;
     }
     is_owner_ = false; // We are just opening, not creating

     // Attach/map the view
     lpBaseAddress_ = MapViewOfFile(
        hMapFile_,            // Handle to map object
        FILE_MAP_ALL_ACCESS,  // Read/write permission
        0, 0, size); // Specify size (can be less than total mapping size)

     if (lpBaseAddress_ == NULL) {
         std::cerr << "SharedMemory [Win]: MapViewOfFile failed during open (" << GetLastError() << ")." << std::endl;
         CloseHandle(hMapFile_); hMapFile_ = NULL;
         key_name_ = ""; size_ = 0; // Reset state
         return false;
     }
     // std::cout << "SharedMemory [Win]: Opened and attached to segment '" << key_or_name << "' at address: " << lpBaseAddress_ << std::endl; // Use logging
     return true;

#else // POSIX specific: Opening an existing segment
     shm_key_ = ftok(key_or_name.c_str(), 'R');
     if (shm_key_ == -1) {
         std::cerr << "SharedMemory [POSIX]: ftok failed for opening '" << key_or_name << "' (" << errno << ": " << strerror(errno) << ")." << std::endl;
         key_name_ = ""; size_ = 0; // Reset state
         return false;
     }
     // std::cout << "SharedMemory [POSIX]: Generated key for opening: " << shm_key_ << std::endl; // Use logging

     // Get the shared memory segment ID (do not create)
     shm_id_ = shmget(shm_key_, 0, 0); // Size 0 and flags 0 to just get the ID

     if (shm_id_ == -1) {
          std::cerr << "SharedMemory [POSIX]: Failed to get segment ID for key " << shm_key_ << " (" << errno << ": " << strerror(errno) << "). Segment might not exist or permissions are wrong." << std::endl;
          shm_key_ = -1; key_name_ = ""; size_ = 0; // Reset state
          return false;
     }
     is_owner_ = false; // We are not the creator
     // std::cout << "SharedMemory [POSIX]: Opened segment with key " << shm_key_ << ", ID: " << shm_id_ << std::endl; // Use logging

     // Attach the shared memory segment
     shm_address_ = shmat(shm_id_, nullptr, 0);
     if (shm_address_ == (void*)-1) {
         std::cerr << "SharedMemory [POSIX]: Failed to attach segment ID " << shm_id_ << " during open (" << errno << ": " << strerror(errno) << ")." << std::endl;
         shm_id_ = -1; shm_address_ = nullptr; shm_key_ = -1; key_name_ = ""; size_ = 0; // Reset state
         return false;
     }
     // std::cout << "SharedMemory [POSIX]: Attached to segment ID " << shm_id_ << " during open at address: " << shm_address_ << std::endl; // Use logging
     return true;
#endif
}


void SharedMemory::Detach() {// Protect instance state

    if (!IsAttached()) { // Check locked by lock_guard
        // std::cout << "SharedMemory: Not attached, nothing to detach." << std::endl; // Use logging
        return;
    }

#ifdef _WIN32
    if (lpBaseAddress_ != nullptr) {
        if (!UnmapViewOfFile(lpBaseAddress_)) {
             std::cerr << "SharedMemory [Win]: Failed to detach view of file (" << GetLastError() << ")." << std::endl;
        } else {
            // std::cout << "SharedMemory [Win]: Detached view of file." << std::endl; // Use logging
        }
        lpBaseAddress_ = nullptr; // Reset pointer
    }
    if (hMapFile_ != nullptr) {
         // On Windows, closing the handle decrement's the reference count.
         // The object is destroyed when the count reaches zero and all views are unmapped.
         if (!CloseHandle(hMapFile_)) {
             std::cerr << "SharedMemory [Win]: Failed to close file mapping handle (" << GetLastError() << ")." << std::endl;
         } else {
             // std::cout << "SharedMemory [Win]: Closed file mapping handle." << std::endl; // Use logging
         }
         hMapFile_ = nullptr; // Reset handle
    }
#else // POSIX
    if (shm_address_ != nullptr && shm_address_ != (void*)-1) {
        if (shmdt(shm_address_) == -1) {
            std::cerr << "SharedMemory [POSIX]: Failed to detach segment (" << errno << ": " << strerror(errno) << ")." << std::endl;
        } else {
            // std::cout << "SharedMemory [POSIX]: Detached segment." << std::endl; // Use logging
        }
        shm_address_ = nullptr; // Reset pointer
    }
    // Note: POSIX shm_id_ persists after detach until shmctl(IPC_RMID) is called by the owner.
    // It's reset in Destroy().
#endif
    key_name_ = ""; // Clear key/name state on detach
    size_ = 0; // Clear size state on detach
    // is_owner_ state is kept, as the instance might still be responsible for destroy
}

bool SharedMemory::Destroy() {
    LSX_LIB::LockManager::LockGuard<std::mutex> lock(mutex_); // Protect instance state

    if (!is_owner_) { // Check locked by lock_guard
         // std::cout << "SharedMemory: Not owner, cannot destroy." << std::endl; // Use logging
         return false;
    }
    // If attached, detach first before trying to destroy (especially on POSIX)
    // Although POSIX allows destroying while attached, it's cleaner to detach first.
    // Windows destroy is implicit on detach.
    if (IsAttached()) { // Check locked by lock_guard
        // std::cout << "SharedMemory: Detaching before destroying..." << std::endl; // Use logging
        Detach(); // Detaching also helps with Windows implicit destroy
        // Note: Detach releases the lock, but Acquire it again immediately after return
        // via lock_guard if needed. Detach handles its own locking internally.
    }

#ifdef _WIN32
    // On Windows, the mapping object is destroyed when the last handle is closed
    // and all views are unmapped. Detach() handles closing the handle and unmapping.
    // We just confirm if the handle/view are null, implying cleanup is done/initiated.
    if (hMapFile_ == nullptr && lpBaseAddress_ == nullptr) {
        // std::cout << "SharedMemory [Win]: Owned shared memory object implicitly destroyed after detach." << std::endl; // Use logging
        is_owner_ = false; // No longer the owner after destruction is handled
        return true;
    } else {
        // This state suggests detach failed or wasn't called correctly before destroy
        std::cerr << "SharedMemory [Win]: Destroy called but handle/view still active? (Internal logic error)" << std::endl;
        return false;
    }
#else // POSIX
    if (shm_id_ != -1) { // Check locked by lock_guard
        // shmctl with IPC_RMID marks the segment for deletion.
        // The segment is actually removed when the last process detaches.
        if (shmctl(shm_id_, IPC_RMID, nullptr) == -1) {
            std::cerr << "SharedMemory [POSIX]: Failed to destroy segment ID " << shm_id_ << " (" << errno << ": " << strerror(errno) << ")." << std::endl;
            return false;
        } else {
            // std::cout << "SharedMemory [POSIX]: Marked segment ID " << shm_id_ << " for destruction." << std::endl; // Use logging
            shm_id_ = -1; // Reset state after marking for deletion
            shm_key_ = -1; // Reset key state
            is_owner_ = false; // No longer the owner
            return true;
        }
    } else {
         // std::cout << "SharedMemory [POSIX]: No valid segment ID to destroy." << std::endl; // Use logging
         is_owner_ = false; // Ensure owner flag is false if ID is invalid
         return false; // No segment to destroy
    }
#endif
}

void* SharedMemory::GetAddress() const {
    if (!IsAttached()) { // Check locked by lock_guard
        // std::cerr << "SharedMemory: Cannot get address, not attached." << std::endl; // Use logging
        return nullptr;
    }
#ifdef _WIN32
    return lpBaseAddress_;
#else // POSIX
    return shm_address_;
#endif
}

size_t SharedMemory::Write(size_t offset, const uint8_t* data, size_t size) {
    if (data == nullptr || size == 0) {
        return 0;
    }
    LSX_LIB::LockManager::LockGuard<std::mutex> lock(mutex_); // Protect access to shared memory pointer and size_

    if (!IsAttached()) { // Check locked by lock_guard
         // std::cerr << "SharedMemory: Write failed. Not attached." << std::endl; // Use logging
         return 0;
    }

    if (offset >= size_) {
        // std::cerr << "SharedMemory: Write out of bounds. Offset: " << offset << ", Size: " << size << ", Segment size: " << size_ << std::endl; // Use logging
        return 0; // Offset is beyond the segment size
    }

     size_t bytes_to_write = std::min(size, size_ - offset);
     if (bytes_to_write > 0) {
         uint8_t* dest = static_cast<uint8_t*>(GetAddress()) + offset; // GetAddress is const and uses the same mutex
         std::memcpy(dest, data, bytes_to_write);
        // std::cout << "SharedMemory: Wrote " << bytes_to_write << " bytes at offset " << offset << "." << std::endl; // Use logging
     }
     return bytes_to_write;
}

size_t SharedMemory::Write(size_t offset, const std::vector<uint8_t>& data) {
    return Write(offset, data.data(), data.size());
}


size_t SharedMemory::Read(size_t offset, uint8_t* buffer, size_t size) const {
    if (buffer == nullptr || size == 0) {
        return 0;
    }
    LSX_LIB::LockManager::LockGuard<std::mutex> lock(mutex_); // Protect access to shared memory pointer and size_

    if (!IsAttached()) { // Check locked by lock_guard
         // std::cerr << "SharedMemory: Read failed. Not attached." << std::endl; // Use logging
         return 0;
    }

    if (offset >= size_) {
        // std::cerr << "SharedMemory: Read out of bounds. Offset: " << offset << ", Size: " << size << ", Segment size: " << size_ << std::endl; // Use logging
        return 0; // Offset is beyond the segment size
    }

    size_t bytes_to_read = std::min(size, size_ - offset);
     if (bytes_to_read > 0) {
        const uint8_t* src = static_cast<const uint8_t*>(GetAddress()) + offset; // GetAddress is const and uses the same mutex
        std::memcpy(buffer, src, bytes_to_read);
        // std::cout << "SharedMemory: Read " << bytes_to_read << " bytes from offset " << offset << "." << std::endl; // Use logging
     }
    return bytes_to_read;
}

std::vector<uint8_t> SharedMemory::Read(size_t offset, size_t size) const {
    std::vector<uint8_t> buffer(size);
    size_t bytes_read = Read(offset, buffer.data(), size);
    buffer.resize(bytes_read);
    return buffer;
}


size_t SharedMemory::GetSize() const {
    LSX_LIB::LockManager::LockGuard<std::mutex> lock(mutex_); // Protect access to size_
    return size_; // Returns the size this instance was created/opened with
    // A more robust POSIX implementation would use shmctl(IPC_STAT) to get the *actual* size.
}

bool SharedMemory::IsAttached() const {
#ifdef _WIN32
    return lpBaseAddress_ != nullptr;
#else // POSIX
    return shm_address_ != nullptr && shm_address_ != reinterpret_cast<void*>(-1);
#endif
}

bool SharedMemory::IsOwner() const {
     LSX_LIB::LockManager::LockGuard<std::mutex> lock(mutex_); // Protect access to is_owner_
    return is_owner_;
}

// Conceptual Exists implementation (platform dependent)
// static bool SharedMemory::Exists(const std::string& key_or_name) {
// #ifdef _WIN32
//     // Windows: Try to open the mapping object without full access
//     HANDLE hMapFile = OpenFileMapping(FILE_MAP_READ, FALSE, key_or_name.c_str());
//     if (hMapFile == NULL) {
//         return GetLastError() != ERROR_FILE_NOT_FOUND;
//     } else {
//         CloseHandle(hMapFile);
//         return true;
//     }
// #else // POSIX
//     // POSIX: Use ftok and shmget with IPC_RMID flag to check existence and permissions
//     key_t key = ftok(key_or_name.c_str(), 'R');
//     if (key == -1) return false; // ftok failed (file might not exist or other issue)
//     int shm_id = shmget(key, 0, 0); // Try to get ID without creating
//     return shm_id != -1; // If shmget succeeds, it exists (and is accessible)
// #endif
// }


} // namespace Memory
} // namespace LIB_LSX