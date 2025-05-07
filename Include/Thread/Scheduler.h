//
// Created by admin on 2025/5/7.
//

#ifndef SCHEDULER_H
#define SCHEDULER_H
#pragma once

#include "ThreadWrapper.h"
#include <vector>
#include <memory>
#include <functional>
#include <mutex> // 添加 mutex

namespace LSX_LIB::Thread
{
    /**
     * @brief 任务调度器，基于 ThreadWrapper 实现一次性及周期性任务
     */
    class Scheduler
    {
    public:
        Scheduler() = default;
        ~Scheduler() { shutdown(); } // 析构时停止所有任务

        // 禁止拷贝和赋值
        Scheduler(const Scheduler&) = delete;
        Scheduler& operator=(const Scheduler&) = delete;

        Scheduler(Scheduler&&) = default;
        Scheduler& operator=(Scheduler&&) = default;

        /**
         * @brief 延迟指定时间后执行一次任务
         * @param delayMs 延迟毫秒数
         * @param func 要执行的可调用对象
         */
        void scheduleOnce(int delayMs, std::function<void()> func);

        /**
         * @brief 每隔指定时间周期性执行任务
         * @param intervalMs 周期间隔毫秒数
         * @param func 要执行的可调用对象
         */
        void schedulePeriodic(int intervalMs, std::function<void()> func);

        /**
         * @brief 停止所有由调度器创建和管理的任务线程
         */
        void shutdown();

    private:
        // 存储所有活跃的任务线程包装器
        // 使用 shared_ptr 管理 ThreadWrapper 对象的生命周期
        std::vector<std::shared_ptr<ThreadWrapper>> tasks_;
        std::mutex tasks_mtx_; // 保护 tasks_ 向量的并发访问
    };
}

#endif //SCHEDULER_H
