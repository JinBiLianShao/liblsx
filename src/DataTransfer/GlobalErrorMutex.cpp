
// ---------- File: GlobalErrorMutex.cpp ----------
#pragma once
#include "GlobalErrorMutex.h"

namespace LSX_LIB::DataTransfer {

    // 定义全局错误输出互斥锁实例
    std::mutex g_error_mutex;

} // namespace LSX_LIB