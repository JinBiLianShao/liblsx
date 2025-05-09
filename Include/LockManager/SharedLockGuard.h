#pragma once
#include <shared_mutex>
#include <chrono>
#include <type_traits>

namespace LIBLSX {
    namespace LockManager {

        template <typename SharedMutexType>
        class SharedLockGuard {
        public:
            explicit SharedLockGuard(SharedMutexType& m)
                : lock_(m) {}

            SharedLockGuard(SharedMutexType& m, std::defer_lock_t) noexcept
                : lock_(m, std::defer_lock) {}

            SharedLockGuard(const SharedLockGuard&) = delete;
            SharedLockGuard& operator=(const SharedLockGuard&) = delete;

            ~SharedLockGuard() noexcept = default;

            bool try_lock_shared() {
                return lock_.try_lock_shared();
            }

            template <typename Rep, typename Period>
            bool try_lock_shared_for(const std::chrono::duration<Rep, Period>& timeout) {
                if constexpr (std::is_same_v<SharedMutexType, std::shared_timed_mutex>) {
                    return lock_.try_lock_shared_for(timeout);
                } else {
                    return lock_.try_lock_shared();
                }
            }

            bool owns_lock() const noexcept {
                return lock_.owns_lock();
            }

        private:
            std::shared_lock<SharedMutexType> lock_;
        };

    } // namespace LockManager
} // namespace LIBLSX