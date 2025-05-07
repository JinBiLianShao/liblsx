// LSXTransportLib: 数据传输工具库（跨平台）
// 命名空间：LSX_LIB

// ---------- File: GlobalErrorMutex.cpp ----------
#include "GlobalErrorMutex.h"

namespace LSX_LIB::DataTransfer {

    // 定义全局错误输出互斥锁实例
    std::mutex g_error_mutex;

} // namespace LSX_LIB