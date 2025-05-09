#pragma once
#include <mutex>
#include <chrono>
#include <type_traits>

namespace LIBLSX {
    namespace LockManager {

        template <typename MutexType>
        class LockGuard {
        public:
            explicit LockGuard(MutexType& m)
                : lock_(m) {}

            LockGuard(MutexType& m, std::defer_lock_t) noexcept
                : lock_(m, std::defer_lock) {}

            LockGuard(const LockGuard&) = delete;
            LockGuard& operator=(const LockGuard&) = delete;

            ~LockGuard() noexcept = default;

            bool try_lock() {
                return lock_.try_lock();
            }

            template <typename Rep, typename Period>
            bool try_lock_for(const std::chrono::duration<Rep, Period>& timeout) {
                if constexpr (std::is_same_v<MutexType, std::timed_mutex> ||
                              std::is_same_v<MutexType, std::recursive_timed_mutex>) {
                    return lock_.try_lock_for(timeout);
                              } else {
                                  return lock_.try_lock();
                              }
            }

            bool owns_lock() const noexcept {
                return lock_.owns_lock();
            }

        private:
            std::unique_lock<MutexType> lock_;
        };

    } // namespace LockManager
} // namespace LIBLSX