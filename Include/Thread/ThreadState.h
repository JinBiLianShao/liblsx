/**
 * @file ThreadState.h
 * @brief 线程状态枚举定义
 * @details 定义了 LSX_LIB::Thread 命名空间下的 ThreadState 枚举，
 * 用于表示 ThreadWrapper 或其他线程管理类中线程的当前状态。
 * 这个枚举提供了一种标准的方式来跟踪线程在其生命周期中的不同阶段。
 * @author 连思鑫（liansixin）
 * @date 2025-5-7
 * @version 1.0
 *
 * ### 核心功能
 * - **状态表示**: 提供清晰的枚举值来表示线程的不同运行状态。
 * - **状态跟踪**: 可用于在线程管理逻辑中跟踪和判断线程的当前情况。
 *
 * ### 使用示例
 *
 * @code
 * #include "ThreadState.h"
 * #include <iostream>
 *
 * int main() {
 * LSX_LIB::Thread::ThreadState currentState = LSX_LIB::Thread::ThreadState::INIT;
 *
 * std::cout << "Current thread state: ";
 * switch (currentState) {
 * case LSX_LIB::Thread::ThreadState::INIT:
 * std::cout << "INIT" << std::endl;
 * break;
 * case LSX_LIB::Thread::ThreadState::RUNNING:
 * std::cout << "RUNNING" << std::endl;
 * break;
 * case LSX_LIB::Thread::ThreadState::PAUSED:
 * std::cout << "PAUSED" << std::endl;
 * break;
 * case LSX_LIB::Thread::ThreadState::STOPPED:
 * std::cout << "STOPPED" << std::endl;
 * break;
 * default:
 * std::cout << "UNKNOWN" << std::endl;
 * break;
 * }
 *
 * // 假设线程状态改变
 * currentState = LSX_LIB::Thread::ThreadState::RUNNING;
 * // ...
 *
 * return 0;
 * }
 * @endcode
 *
 * ### 注意事项
 * - **状态转换**: 枚举本身只定义了状态，具体的线程管理类需要负责实现状态之间的转换逻辑。
 * - **线程安全**: 如果多个线程需要访问或修改线程状态，需要额外的同步机制（如互斥锁）来保证线程安全。
 */

//
// Created by admin on 2025/5/7.
//

#ifndef THREADSTATE_H
#define THREADSTATE_H
#pragma once

/**
 * @brief LSX 库的根命名空间。
 */
namespace LSX_LIB
{
    /**
     * @brief 线程相关的命名空间。
     * 包含线程管理和线程间通信相关的类和工具。
     */
    namespace Thread
    {
        /**
         * @brief 线程状态枚举。
         * 定义了线程在其生命周期中可能处于的不同状态。
         */
        enum class ThreadState
        {
            /**
             * @brief 初始化完成状态。
             * 线程对象已创建，任务已设置，但线程尚未启动。
             */
            INIT,

            /**
             * @brief 运行中状态。
             * 线程正在执行其任务。
             */
            RUNNING,

            /**
             * @brief 已暂停状态。
             * 线程的执行已暂停，等待恢复。
             * (注意：此状态的实际支持取决于具体的线程管理实现)。
             */
            PAUSED,

            /**
             * @brief 已停止或运行结束状态。
             * 线程已完成其任务正常退出，或者已被请求停止并已退出。
             */
            STOPPED
        };
    } // namespace Thread
} // namespace LSX_LIB
#endif //THREADSTATE_H
