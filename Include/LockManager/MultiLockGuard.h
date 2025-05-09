#pragma once
#include <mutex>

namespace LIBLSX {
    namespace LockManager {

        template <typename... Mutexes>
        class MultiLockGuard {
        public:
            explicit MultiLockGuard(Mutexes&... ms)
                : lock_(ms...) {}

            MultiLockGuard(const MultiLockGuard&) = delete;
            MultiLockGuard& operator=(const MultiLockGuard&) = delete;

            ~MultiLockGuard() noexcept = default;

        private:
            std::scoped_lock<Mutexes...> lock_;
        };

    } // namespace LockManager
} // namespace LIBLSX