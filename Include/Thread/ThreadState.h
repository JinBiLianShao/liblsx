//
// Created by admin on 2025/5/7.
//

#ifndef THREADSTATE_H
#define THREADSTATE_H
#pragma once

namespace LSX_LIB::Thread
{

    /**
     * @brief 线程状态枚举
     */
    enum class ThreadState {
        INIT,     // 初始化完成，任务已设置但尚未启动
        RUNNING,  // 正在运行
        PAUSED,   // 已暂停
        STOPPED   // 已停止或运行结束
    };

}
#endif //THREADSTATE_H
