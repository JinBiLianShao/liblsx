#pragma once
#include "FixedSizeQueue.h"

#include <cstring>
#include <algorithm>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <iostream>
// #include <iostream>
#include "LockGuard.h"
#include "MultiLockGuard.h"


namespace LIB_LSX
{
    namespace Memory
    {
        uint8_t* FixedSizeQueue::get_block_address(size_t index)
        {
            if (index >= block_count_)
            {
                std::cerr << "FixedSizeQueue: get_block_address called with out-of-bounds index: " << index <<
                    std::endl;
                throw std::out_of_range(
                    "FixedSizeQueue: Block index out of range in get_block_address. This indicates a logical error in head/tail management.");
            }
            return buffer_.data() + index * block_size_;
        }

        const uint8_t* FixedSizeQueue::get_block_address(size_t index) const
        {
            if (index >= block_count_)
            {
                std::cerr << "FixedSizeQueue: get_block_address (const) called with out-of-bounds index: " << index <<
                    std::endl;
                throw std::out_of_range(
                    "FixedSizeQueue: Block index out of range in get_block_address (const). This indicates a logical error in head/tail management.");
            }
            return buffer_.data() + index * block_size_;
        }


        FixedSizeQueue::FixedSizeQueue(size_t block_size, size_t block_count)
            : block_size_(block_size), block_count_(block_count), head_(0), tail_(0), current_size_(0)
        {
            if (block_size == 0 || block_count == 0)
            {
                throw std::invalid_argument("FixedSizeQueue: block_size and block_count must be greater than 0");
            }
            try
            {
                buffer_.resize(block_size_ * block_count_);
                // std::cout << "FixedSizeQueue: Created with block_size=" << block_size_
                //           << ", block_count=" << block_count_
                //           << ", total_size=" << buffer_.size() << std::endl;
            }
            catch (const std::bad_alloc& e)
            {
                std::cerr << "FixedSizeQueue: Failed to allocate buffer: " << e.what() << std::endl;
                buffer_.clear();
                block_size_ = 0;
                block_count_ = 0;
                throw;
            }
        }

        void FixedSizeQueue::Clear()
        {
            LIBLSX::LockManager::LockGuard<std::mutex> lock(mutex_);
            head_ = 0;
            tail_ = 0;
            current_size_ = 0;
            // std::cout << "FixedSizeQueue: Cleared." << std::endl;
            cv_write_.notify_all();
            cv_read_.notify_all();
        }

        bool FixedSizeQueue::Put(const uint8_t* data, size_t data_size)
        {
            if (data == nullptr || data_size != block_size_)
            {
                // std::cerr << "FixedSizeQueue: Put failed. Invalid data or size mismatch." << std::endl;
                return false;
            }
            LIBLSX::LockManager::LockGuard<std::mutex> lock(mutex_);

            if (current_size_ == block_count_)
            {
                // IsFull_unsafe()
                // std::cout << "FixedSizeQueue: Queue is full, cannot Put." << std::endl;
                return false;
            }

            // tail_ is the index of the next empty slot
            uint8_t* dest = get_block_address(tail_);
            std::memcpy(dest, data, block_size_);

            tail_ = (tail_ + 1) % block_count_; // Move tail circularly
            current_size_++;

            // std::cout << "FixedSizeQueue: Put block. New tail: " << tail_ << ", New size: " << current_size_ << std::endl;
            cv_read_.notify_one();
            return true;
        }

        bool FixedSizeQueue::Put(const std::vector<uint8_t>& data)
        {
            return Put(data.data(), data.size());
        }


        bool FixedSizeQueue::Get(uint8_t* buffer, size_t buffer_size)
        {
            if (buffer == nullptr || buffer_size < block_size_)
            {
                // std::cerr << "FixedSizeQueue: Get failed. Invalid buffer or size too small." << std::endl;
                return false;
            }
            LIBLSX::LockManager::LockGuard<std::mutex> lock(mutex_);

            if (current_size_ == 0)
            {
                // IsEmpty_unsafe()
                // std::cout << "FixedSizeQueue: Queue is empty, cannot Get." << std::endl;
                return false;
            }

            // head_ is the index of the block to get
            const uint8_t* src = get_block_address(head_);
            std::memcpy(buffer, src, block_size_);

            head_ = (head_ + 1) % block_count_; // Move head circularly
            current_size_--;

            // std::cout << "FixedSizeQueue: Got block. New head: " << head_ << ", New size: " << current_size_ << std::endl;
            cv_write_.notify_one();
            return true;
        }

        std::optional<std::vector<uint8_t>> FixedSizeQueue::Get()
        {
            LIBLSX::LockManager::LockGuard<std::mutex> lock(mutex_);
            if (current_size_ == 0)
            {
                // IsEmpty_unsafe()
                // std::cout << "FixedSizeQueue: Queue is empty, cannot Get (vector)." << std::endl;
                return std::nullopt;
            }

            std::vector<uint8_t> block_data(block_size_);
            const uint8_t* src = get_block_address(head_);
            std::memcpy(block_data.data(), src, block_size_);

            head_ = (head_ + 1) % block_count_;
            current_size_--;

            // std::cout << "FixedSizeQueue: Got block (vector). New head: " << head_ << ", New size: " << current_size_ << std::endl;
            cv_write_.notify_one();
            return block_data;
        }


        bool FixedSizeQueue::Peek(uint8_t* buffer, size_t buffer_size) const
        {
            if (buffer == nullptr || buffer_size < block_size_)
            {
                // std::cerr << "FixedSizeQueue: Peek failed. Invalid buffer or size too small." << std::endl;
                return false;
            }
            LIBLSX::LockManager::LockGuard<std::mutex> lock(mutex_);

            if (current_size_ == 0)
            {
                // IsEmpty_unsafe()
                // std::cout << "FixedSizeQueue: Queue is empty, cannot Peek." << std::endl;
                return false;
            }

            const uint8_t* src = get_block_address(head_);
            std::memcpy(buffer, src, block_size_);

            // std::cout << "FixedSizeQueue: Peeked block at head: " << head_ << std::endl;
            return true;
        }

        std::optional<std::vector<uint8_t>> FixedSizeQueue::Peek() const
        {
            LIBLSX::LockManager::LockGuard<std::mutex> lock(mutex_);
            if (current_size_ == 0)
            {
                // IsEmpty_unsafe()
                // std::cout << "FixedSizeQueue: Queue is empty, cannot Peek (vector)." << std::endl;
                return std::nullopt;
            }

            std::vector<uint8_t> block_data(block_size_);
            const uint8_t* src = get_block_address(head_);
            std::memcpy(block_data.data(), src, block_size_);

            // std::cout << "FixedSizeQueue: Peeked block (vector) at head: " << head_ << std::endl;
            return block_data;
        }


        // --- Data Access Functions (Blocking) ---
        bool FixedSizeQueue::GetBlocking(uint8_t* buffer, size_t buffer_size, long timeout_ms)
        {
            if (buffer == nullptr || buffer_size < block_size_)
            {
                // std::cerr << "FixedSizeQueue: GetBlocking failed. Invalid buffer or size too small." << std::endl;
                return false;
            }
            std::unique_lock<std::mutex> lock(mutex_);

            // Lambda for IsEmpty condition, used by condition variable
            auto queue_not_empty = [&] { return current_size_ != 0; };

            if (current_size_ == 0)
            {
                if (timeout_ms == 0)
                {
                    // std::cout << "FixedSizeQueue: GetBlocking (non-blocking) empty." << std::endl;
                    return false;
                }
                else if (timeout_ms > 0)
                {
                    auto duration = std::chrono::milliseconds(timeout_ms);
                    if (!cv_read_.wait_for(lock, duration, queue_not_empty))
                    {
                        // std::cout << "FixedSizeQueue: GetBlocking timed out." << std::endl;
                        return false;
                    }
                }
                else
                {
                    // Infinite wait
                    // std::cout << "FixedSizeQueue: GetBlocking waiting indefinitely." << std::endl;
                    cv_read_.wait(lock, queue_not_empty);
                }
                // Recheck after wait (spurious wakeup or other thread might have acted)
                if (current_size_ == 0)
                {
                    // std::cout << "FixedSizeQueue: GetBlocking woke up but still empty." << std::endl;
                    return false;
                }
            }

            const uint8_t* src = get_block_address(head_);
            std::memcpy(buffer, src, block_size_);

            head_ = (head_ + 1) % block_count_;
            current_size_--;

            // std::cout << "FixedSizeQueue: GetBlocking got block. New head: " << head_ << ", New size: " << current_size_ << std::endl;
            lock.unlock(); // Unlock before notifying to avoid lock contention
            cv_write_.notify_one();
            return true;
        }

        std::optional<std::vector<uint8_t>> FixedSizeQueue::GetBlocking(long timeout_ms)
        {
            std::unique_lock<std::mutex> lock(mutex_);

            auto queue_not_empty = [&] { return current_size_ != 0; };

            if (current_size_ == 0)
            {
                if (timeout_ms == 0)
                {
                    // std::cout << "FixedSizeQueue: GetBlocking (non-blocking, vector) empty." << std::endl;
                    return std::nullopt;
                }
                else if (timeout_ms > 0)
                {
                    auto duration = std::chrono::milliseconds(timeout_ms);
                    if (!cv_read_.wait_for(lock, duration, queue_not_empty))
                    {
                        // std::cout << "FixedSizeQueue: GetBlocking (vector) timed out." << std::endl;
                        return std::nullopt;
                    }
                }
                else
                {
                    // std::cout << "FixedSizeQueue: GetBlocking (vector) waiting indefinitely." << std::endl;
                    cv_read_.wait(lock, queue_not_empty);
                }
                if (current_size_ == 0)
                {
                    // std::cout << "FixedSizeQueue: GetBlocking (vector) woke up but still empty." << std::endl;
                    return std::nullopt;
                }
            }

            std::vector<uint8_t> block_data(block_size_);
            const uint8_t* src = get_block_address(head_);
            std::memcpy(block_data.data(), src, block_size_);

            head_ = (head_ + 1) % block_count_;
            current_size_--;

            // std::cout << "FixedSizeQueue: GetBlocking got block (vector). New head: " << head_ << ", New size: " << current_size_ << std::endl;
            lock.unlock();
            cv_write_.notify_one();
            return block_data;
        }

        bool FixedSizeQueue::PutBlocking(const uint8_t* data, size_t data_size, long timeout_ms)
        {
            if (data == nullptr || data_size != block_size_)
            {
                // std::cerr << "FixedSizeQueue: PutBlocking failed. Invalid data or size mismatch." << std::endl;
                return false;
            }
            std::unique_lock<std::mutex> lock(mutex_);

            auto queue_not_full = [&] { return current_size_ != block_count_; };

            if (current_size_ == block_count_)
            {
                if (timeout_ms == 0)
                {
                    // std::cout << "FixedSizeQueue: PutBlocking (non-blocking) full." << std::endl;
                    return false;
                }
                else if (timeout_ms > 0)
                {
                    auto duration = std::chrono::milliseconds(timeout_ms);
                    if (!cv_write_.wait_for(lock, duration, queue_not_full))
                    {
                        // std::cout << "FixedSizeQueue: PutBlocking timed out." << std::endl;
                        return false;
                    }
                }
                else
                {
                    // std::cout << "FixedSizeQueue: PutBlocking waiting indefinitely." << std::endl;
                    cv_write_.wait(lock, queue_not_full);
                }
                if (current_size_ == block_count_)
                {
                    // std::cout << "FixedSizeQueue: PutBlocking woke up but still full." << std::endl;
                    return false;
                }
            }

            uint8_t* dest = get_block_address(tail_);
            std::memcpy(dest, data, block_size_);

            tail_ = (tail_ + 1) % block_count_;
            current_size_++;

            // std::cout << "FixedSizeQueue: PutBlocking put block. New tail: " << tail_ << ", New size: " << current_size_ << std::endl;
            lock.unlock();
            cv_read_.notify_one();
            return true;
        }

        bool FixedSizeQueue::PutBlocking(const std::vector<uint8_t>& data, long timeout_ms)
        {
            return PutBlocking(data.data(), data.size(), timeout_ms);
        }


        // --- Status Functions ---
        // These are called with lock held by public methods, or by Put/Get/Peek/Clear themselves.
        // If they were public and called directly, they would need their own locks.
        // For internal use (like in condition variable lambdas or before operations),
        // they can be _unsafe versions if the lock is already held.
        // For simplicity and safety, current IsEmpty/IsFull public methods in .h take locks.
        // The lambdas for CVs correctly capture `this` and access `current_size_` while lock is held.

        bool FixedSizeQueue::IsEmpty() const
        {
            LIBLSX::LockManager::LockGuard<std::mutex> lock(mutex_);
            return current_size_ == 0;
        }

        bool FixedSizeQueue::IsFull() const
        {
            LIBLSX::LockManager::LockGuard<std::mutex> lock(mutex_);
            return current_size_ == block_count_;
        }

        size_t FixedSizeQueue::Size() const
        {
            LIBLSX::LockManager::LockGuard<std::mutex> lock(mutex_);
            return current_size_;
        }
    } // namespace Memory
} // namespace LIB_LSX
