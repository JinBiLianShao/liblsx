//
// Created by admin on 2025/5/7.
//

#ifndef ITHREADPOOL_H
#define ITHREADPOOL_H
#pragma once

#include <functional>
#include <memory>

namespace LSX_LIB::Thread {

    /**
     * @brief 线程池接口定义 (占位)
     * 用于未来集成线程池功能
     */
    class IThreadPool {
    public:
        virtual ~IThreadPool() = default;

        /**
         * @brief 向线程池提交一个任务
         * @param task 要执行的可调用对象
         */
        virtual void enqueue(std::function<void()> task) = 0;

        /**
         * @brief 关闭线程池，等待所有任务完成
         */
        virtual void shutdown() = 0;
    };

} // namespace LSX_LIB::Thread

#endif //ITHREADPOOL_H
