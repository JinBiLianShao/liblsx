#include "Condition.h" // 包含对应的头文件

namespace LIBLSX {
    namespace LockManager {

        // notify_one 的实现
        void Condition::notify_one() noexcept {
            cv_.notify_one();
        }

        // notify_all 的实现
        void Condition::notify_all() noexcept {
            cv_.notify_all();
        }

    } // namespace LockManager
} // namespace LIBLSX