// LSXTransportLib: 数据传输工具库（跨平台）
// 命名空间：LSX_LIB

// ---------- File: GlobalErrorMutex.h ----------
#ifndef LSX_GLOBAL_ERROR_MUTEX_H
#define LSX_GLOBAL_ERROR_MUTEX_H
#pragma once
#include <mutex>

namespace LSX_LIB::DataTransfer {

    // 用于保护全局错误输出的互斥锁
    extern std::mutex g_error_mutex;

} // namespace LSX_LIB

#endif // LSX_GLOBAL_ERROR_MUTEX_H