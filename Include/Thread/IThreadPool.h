/**
 * @file IThreadPool.h
 * @brief 线程池接口定义
 * @details 定义了 LSX_LIB::Thread 命名空间下的抽象基类 IThreadPool，
 * 这是一个占位接口，用于未来集成线程池功能。
 * 它提供了一套标准的接口，允许用户向线程池提交可执行的任务，并管理线程池的生命周期（关闭）。
 * 具体的实现将负责线程的创建、管理、任务调度和执行。
 * @author 连思鑫（liansixin）
 * @date 2025-5-7
 * @version 1.0 (占位接口版本)
 *
 * ### 核心功能
 * - **接口标准化**: 定义了向线程池提交任务和关闭线程池的标准接口。
 * - **任务提交**: 允许通过 `enqueue` 方法提交任意的可调用对象作为任务。
 * - **线程池管理**: 提供 `shutdown` 方法用于优雅地关闭线程池，等待已提交任务完成。
 * - **多态支持**: 作为抽象基类，允许通过基类指针操作不同的具体线程池实现。
 *
 * ### 使用示例
 *
 * @code
 * #include "IThreadPool.h"
 * #include <iostream>
 * #include <vector>
 * #include <chrono>
 * #include <thread>
 * #include <future> // 用于演示任务结果获取 (如果具体实现支持)
 *
 * // 示例：一个简单的线程池实现 (仅为演示概念)
 * // 实际的线程池需要更复杂的任务队列、线程管理和同步机制
 * class SimpleThreadPool : public LSX_LIB::Thread::IThreadPool {
 * public:
 * // 构造函数等需要具体实现线程管理
 * SimpleThreadPool(size_t num_threads) {
 * // 示例：创建指定数量的工作线程
 * // ... 实际实现 ...
 * }
 *
 * ~SimpleThreadPool() override {
 * // 示例：确保线程池已关闭
 * shutdown();
 * }
 *
 * void enqueue(std::function<void()> task) override {
 * // 示例：将任务添加到队列并通知工作线程
 * // ... 实际实现 ...
 * std::cout << "任务已提交到线程池。" << std::endl;
 * // 在实际线程池中，任务会被工作线程异步执行
 * task(); // 示例中直接执行，实际会放入队列
 * }
 *
 * void shutdown() override {
 * // 示例：通知工作线程退出并等待它们完成
 * // ... 实际实现 ...
 * std::cout << "线程池正在关闭..." << std::endl;
 * // ... 等待线程 ...
 * std::cout << "线程池已关闭。" << std::endl;
 * }
 * };
 *
 * // 示例任务函数
 * void simple_task(int id) {
 * std::cout << "线程 " << std::this_thread::get_id() << " 正在执行任务 " << id << std::endl;
 * std::this_thread::sleep_for(std::chrono::milliseconds(100));
 * std::cout << "任务 " << id << " 完成。" << std::endl;
 * }
 *
 * int main() {
 * // 创建一个线程池实例 (例如，2个线程)
 * // auto thread_pool = std::make_unique<SimpleThreadPool>(2);
 *
 * // 提交任务到线程池
 * // thread_pool->enqueue([](){ simple_task(1); });
 * // thread_pool->enqueue([](){ simple_task(2); });
 * // thread_pool->enqueue([](){ simple_task(3); });
 *
 * // ... 在实际应用中，主线程可以继续做其他事情 ...
 *
 * // 在程序结束前关闭线程池，等待所有任务完成
 * // thread_pool->shutdown();
 *
 * std::cout << "主线程退出。" << std::endl;
 * return 0;
 * }
 * @endcode
 *
 * ### 注意事项
 * - **抽象接口**: IThreadPool 是一个纯虚类，不能直接实例化，需要具体的派生类来实现其方法。
 * - **具体实现**: 实际的线程管理、任务队列、同步机制和调度逻辑需要在派生类中实现。
 * - **任务类型**: `enqueue` 方法接受一个 `std::function<void()>`，意味着提交的任务不能直接返回结果或接受参数（除非通过 lambda 捕获）。如果需要更灵活的任务类型（如带返回值或参数），接口可能需要修改。
 * - **异常处理**: 具体实现需要考虑如何处理任务执行过程中抛出的异常。
 * - **关闭机制**: `shutdown` 方法应确保所有已提交的任务在线程池关闭前得到执行。可能还需要提供一个立即停止（取消未执行任务）的机制。
 * - **线程安全**: 具体的实现类必须保证 `enqueue` 和 `shutdown` 方法的线程安全。
 */

//
// Created by admin on 2025/5/7.
//

#ifndef ITHREADPOOL_H
#define ITHREADPOOL_H
#pragma once

#include <functional> // 包含 std::function
#include <memory> // 包含 std::unique_ptr 等智能指针

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
         * @brief 线程池接口定义 (占位)。
         * 这是一个抽象基类，定义了线程池的标准接口。
         * 具体的实现类将提供实际的线程管理、任务调度和执行机制。
         * 用于未来集成线程池功能。
         */
        class IThreadPool
        {
        public:
            /**
             * @brief 虚析构函数。
             * 确保通过基类指针删除派生类对象时，能够正确调用派生类的析构函数，释放所有资源。
             * 默认实现。
             */
            virtual ~IThreadPool() = default;

            /**
             * @brief 向线程池提交一个任务。
             * 将一个可调用对象（函数、lambda、函数对象等）作为一个任务添加到线程池的任务队列中。
             * 线程池中的一个可用工作线程将异步地执行此任务。
             *
             * @param task 要执行的可调用对象，其签名必须是 `void()`（无参数，无返回值）。
             */
            virtual void enqueue(std::function<void()> task) = 0;

            /**
             * @brief 关闭线程池。
             * 启动线程池的关闭过程。通常，这意味着不再接受新的任务，并等待所有已提交但尚未执行或正在执行的任务完成。
             * 此方法可能是阻塞的，直到所有任务完成且工作线程退出。
             */
            virtual void shutdown() = 0;
        };
    } // namespace LSX_LIB::Thread
} // namespace LSX_LIB

#endif //ITHREADPOOL_H
