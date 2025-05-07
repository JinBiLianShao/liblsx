/**
 * @file Scheduler.h
 * @brief 任务调度器
 * @details 定义了 LSX_LIB::Thread 命名空间下的 Scheduler 类，
 * 这是一个任务调度器，利用 ThreadWrapper 类来管理一次性延迟任务和周期性任务。
 * 它允许用户提交一个函数，指定延迟时间或周期间隔，然后在独立的线程中执行这些任务。
 * Scheduler 负责创建和管理这些任务线程的生命周期，并在析构或调用 shutdown 时停止所有正在运行的任务。
 * 使用互斥锁确保在多线程环境下安全地管理任务列表。
 * @author 连思鑫（liansixin）
 * @date 2025-5-7
 * @version 1.0
 *
 * ### 核心功能
 * - **一次性任务调度**: 支持延迟指定时间后执行一次任务。
 * - **周期性任务调度**: 支持每隔指定时间周期性地执行任务。
 * - **任务线程管理**: 为每个任务创建并管理独立的线程。
 * - **优雅关闭**: 在析构或调用 shutdown 时，会尝试停止所有由调度器管理的任务线程。
 * - **线程安全**: 使用互斥锁保护内部的任务列表，支持在多线程环境中提交和停止任务。
 *
 * ### 使用示例
 *
 * @code
 * #include "Scheduler.h"
 * #include <iostream>
 * #include <chrono>
 * #include <thread>
 *
 * // 示例任务函数
 * void my_once_task() {
 * std::cout << "一次性任务在线程 " << std::this_thread::get_id() << " 中执行。" << std::endl;
 * }
 *
 * void my_periodic_task() {
 * std::cout << "周期性任务在线程 " << std::this_thread::get_id() << " 中执行。" << std::endl;
 * }
 *
 * int main() {
 * LSX_LIB::Thread::Scheduler scheduler;
 *
 * std::cout << "调度一个一次性任务，延迟 2 秒执行..." << std::endl;
 * scheduler.scheduleOnce(2000, my_once_task);
 *
 * std::cout << "调度一个周期性任务，每隔 1 秒执行..." << std::endl;
 * scheduler.schedulePeriodic(1000, my_periodic_task);
 *
 * std::cout << "主线程等待 5 秒..." << std::endl;
 * std::this_thread::sleep_for(std::chrono::seconds(5));
 *
 * std::cout << "主线程调用 shutdown 停止所有任务..." << std::endl;
 * scheduler.shutdown();
 *
 * std::cout << "主线程退出。" << std::endl;
 * return 0;
 * }
 * @endcode
 *
 * ### 注意事项
 * - **线程创建**: 每个调度的任务都会创建一个新的 ThreadWrapper 对象，并在其内部启动一个新线程。如果调度大量任务，可能会消耗较多系统资源。
 * - **任务生命周期**: 任务线程的生命周期由 Scheduler 管理。一旦 Scheduler 对象被销毁或调用 shutdown，所有关联的任务线程将被请求停止。
 * - **任务停止**: `shutdown` 方法会向所有任务线程发送停止请求，并等待它们完成。如果任务函数内部没有检查停止标志或没有适当的退出机制，任务线程可能不会立即停止。
 * - **异常处理**: 任务函数内部抛出的异常需要在任务函数自身或 ThreadWrapper 内部进行处理，否则可能导致程序崩溃。
 * - **移动语义**: 类支持移动构造和移动赋值，但不允许拷贝，以避免多个 Scheduler 对象管理同一组任务线程。
 */

//
// Created by admin on 2025/5/7.
//

#ifndef SCHEDULER_H
#define SCHEDULER_H
#pragma once

#include "ThreadWrapper.h" // 包含 ThreadWrapper 类定义
#include <vector> // 包含 std::vector
#include <memory> // 包含 std::shared_ptr
#include <functional> // 包含 std::function
#include <mutex> // 添加 mutex，包含 std::mutex

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
         * @brief 任务调度器。
         * 基于 ThreadWrapper 实现，用于调度一次性延迟任务和周期性任务。
         * 管理任务线程的创建、执行和停止。
         */
        class Scheduler
        {
        public:
            /**
             * @brief 默认构造函数。
             * 初始化 Scheduler 对象。
             */
            Scheduler() = default;

            /**
             * @brief 析构函数。
             * 在 Scheduler 对象销毁时自动调用 shutdown()，停止所有管理的任务线程。
             */
            ~Scheduler() { shutdown(); } // 析构时停止所有任务

            // 禁止拷贝和赋值，防止多个 Scheduler 对象管理同一组任务线程，导致双重释放等问题
            /**
             * @brief 禁用拷贝构造函数。
             */
            Scheduler(const Scheduler&) = delete;
            /**
             * @brief 禁用赋值运算符。
             */
            Scheduler& operator=(const Scheduler&) = delete;

            // 允许移动构造和移动赋值，支持资源的转移
            /**
             * @brief 默认移动构造函数。
             */
            Scheduler(Scheduler&&) = default;
            /**
             * @brief 默认移动赋值运算符。
             */
            Scheduler& operator=(Scheduler&&) = default;

            /**
             * @brief 延迟指定时间后执行一次任务。
             * 创建一个 ThreadWrapper 对象，用于在指定延迟后执行一次给定的可调用对象。
             * 任务线程的生命周期由 Scheduler 管理。
             *
             * @param delayMs 延迟执行的毫秒数。
             * @param func 要执行的可调用对象，其签名必须是 `void()`。
             */
            void scheduleOnce(int delayMs, std::function<void()> func);

            /**
             * @brief 每隔指定时间周期性执行任务。
             * 创建一个 ThreadWrapper 对象，用于每隔指定间隔周期性地执行给定的可调用对象。
             * 任务线程的生命周期由 Scheduler 管理。
             *
             * @param intervalMs 周期性执行的时间间隔，单位为毫秒。
             * @param func 要执行的可调用对象，其签名必须是 `void()`。
             */
            void schedulePeriodic(int intervalMs, std::function<void()> func);

            /**
             * @brief 停止所有由调度器创建和管理的任务线程。
             * 向所有活跃的任务线程发送停止请求，并等待它们完成执行后加入。
             * 此方法是阻塞的，直到所有任务线程都已停止。
             */
            void shutdown();

        private:
            /**
             * @brief 存储所有活跃的任务线程包装器。
             * 使用 std::shared_ptr 管理 ThreadWrapper 对象的生命周期，允许多个地方（例如，如果需要取消特定任务）共享对 ThreadWrapper 的引用。
             */
            std::vector<std::shared_ptr<ThreadWrapper>> tasks_;
            /**
             * @brief 互斥锁。
             * 用于保护 tasks_ 向量的并发访问，确保在多线程环境下添加、移除或遍历任务列表时的线程安全。
             */
            std::mutex tasks_mtx_; // 保护 tasks_ 向量的并发访问
        };
    } // namespace Thread
} // namespace LSX_LIB

#endif //SCHEDULER_H
