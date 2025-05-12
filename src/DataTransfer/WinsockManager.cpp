
// ---------- File: WinsockManager.cpp ----------
#pragma once
#include "WinsockManager.h"
#include "GlobalErrorMutex.h" // 包含全局错误锁头文件

namespace LSX_LIB::DataTransfer {

#ifdef _WIN32
    // 在一个编译单元中定义静态实例
    // 这确保 Winsock 在程序启动时初始化
    // 并在程序结束时清理
    static WinsockManager globalWinsockManager;
#endif

} // namespace LSX_LIB