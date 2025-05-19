#include "Condition.h"

namespace LSX_LIB {
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