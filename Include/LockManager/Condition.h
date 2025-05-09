#pragma once
#include <condition_variable> // For std::condition_variable_any

namespace LIBLSX {
    namespace LockManager {

        class Condition {
        public:
            // 等待条件（无谓词）。
            // LockType 必须满足 BasicLockable 要求。作为模板方法，其实现必须在头文件中。
            template <typename LockType>
            void wait(LockType& lock) {
                cv_.wait(lock);
            }

            // 等待条件（带谓词），直到 pred() 返回 true。
            // LockType 必须满足 BasicLockable 要求。作为模板方法，其实现必须在头文件中。
            template <typename LockType, typename Predicate>
            void wait(LockType& lock, Predicate pred) {
                cv_.wait(lock, pred);
            }

            // 通知一个等待的线程。声明，实现将在 .cpp 文件中。
            void notify_one() noexcept;

            // 通知所有等待的线程。声明，实现将在 .cpp 文件中。
            void notify_all() noexcept;

        private:
            std::condition_variable_any cv_;
        };

    } // namespace LockManager
} // namespace LIBLSX