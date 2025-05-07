//
// Created by admin on 2025/5/7.
//

#ifndef THREADWRAPPER_H
#define THREADWRAPPER_H
#pragma once

#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <functional>
#include <memory>
#include "ThreadState.h"
#include "IThreadPool.h"
#include "ICommunicator.h"

namespace LSX_LIB::Thread {

/**
 * @brief C++ 单线程封装类，提供生命周期管理和状态控制
 */
class ThreadWrapper {
public:
    ThreadWrapper();
    ~ThreadWrapper();

    // 禁止拷贝和赋值，线程对象不可拷贝
    ThreadWrapper(const ThreadWrapper&) = delete;
    ThreadWrapper& operator=(const ThreadWrapper&) = delete;

    ThreadWrapper(ThreadWrapper&&) = default;
    ThreadWrapper& operator=(ThreadWrapper&&) = default;


    /**
     * @brief 绑定要在此线程中执行的任务 (可调用对象及参数)
     * @tparam F 可调用对象类型
     * @tparam Args 参数类型
     * @param f 可调用对象
     * @param args 参数
     */
    template<typename F, typename... Args>
    void setTask(F&& f, Args&&... args) {
        std::lock_guard<std::mutex> lk(mtx_); // 保护 task_ 和 state_ 的访问
        task_ = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
        state_.store(ThreadState::INIT);
    }

    /**
     * @brief 启动线程并执行绑定的任务
     * @param detached 如果为 true，线程将分离；否则默认可join
     */
    void start(bool detached = false);

    /**
     * @brief 请求停止线程并等待其退出 (join)
     */
    void stop();

    /**
     * @brief 暂停线程执行任务
     */
    void pause();

    /**
     * @brief 恢复线程执行任务
     */
    void resume();

    /**
     * @brief 停止当前线程（如果正在运行）并重新启动
     * @param detached 如果为 true，新线程将分离；否则默认可join
     */
    void restart(bool detached = false);

    /**
     * @brief 获取当前线程状态
     * @return 线程状态枚举
     */
    ThreadState getState() const { return state_.load(); }

    // --- 扩展接口 ---

    /**
     * @brief 设置线程池实例 (用于未来扩展，当前 ThreadWrapper 不直接使用)
     * @param pool 线程池共享指针
     */
    void setThreadPool(std::shared_ptr<IThreadPool> pool) { threadPool_ = pool; }

    /**
     * @brief 设置线程间通信实例 (用于未来扩展，当前 ThreadWrapper 不直接使用)
     * @param comm 通信器共享指针
     */
    void setCommunicator(std::shared_ptr<ICommunicator> comm) { communicator_ = comm; }

private:
    /**
     * @brief 线程实际执行函数
     * 负责任务的执行、暂停/恢复和停止逻辑
     */
    void threadFunc();

    std::thread            worker_;         // 工作线程对象
    std::function<void()>  task_;           // 要执行的任务
    mutable std::mutex     mtx_;            // 互斥锁，保护 task_ 和条件变量
    std::condition_variable cv_;            // 条件变量，用于暂停/恢复和等待任务绑定
    std::atomic<bool>      stopFlag_;       // 停止标志
    std::atomic<bool>      pauseFlag_;      // 暂停标志
    std::atomic<ThreadState> state_;        // 线程状态

    std::shared_ptr<IThreadPool>   threadPool_;   // 线程池接口 (占位)
    std::shared_ptr<ICommunicator> communicator_; // 通信接口 (占位)
};

} // namespace LSX_LIB::Thread

#endif //THREADWRAPPER_H
