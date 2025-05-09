/**
 * @file ThreadWrapper.h
 * @brief C++ 单线程封装类
 * @details 定义了 LSX_LIB::Thread 命名空间下的 ThreadWrapper 类，
 * 这是一个用于封装和管理单个 C++ 标准线程的类。
 * 它提供了线程生命周期管理（启动、停止、暂停、恢复）和状态跟踪的功能。
 * 用户可以通过 `setTask` 方法绑定一个可调用对象作为线程的执行任务，
 * 并通过 `start` 方法启动线程。类内部使用原子标志和条件变量来实现线程的停止、暂停和恢复控制，
 * 并使用互斥锁保护共享资源。支持移动语义，但禁止拷贝，以确保线程资源的唯一性。
 * 包含了未来扩展的线程池和线程间通信接口的占位。
 * @author 连思鑫（liansixin）
 * @date 2025-5-7
 * @version 1.0
 *
 * ### 核心功能
 * - **线程封装**: 封装 `std::thread` 对象，简化线程管理。
 * - **任务绑定**: 支持绑定任意可调用对象作为线程任务。
 * - **生命周期控制**: 提供 `start`, `stop`, `pause`, `resume`, `restart` 方法控制线程。
 * - **状态跟踪**: 使用 `ThreadState` 枚举和原子变量跟踪线程状态。
 * - **优雅停止**: 通过原子标志和条件变量实现可控的停止机制。
 * - **暂停/恢复**: 利用条件变量实现线程的暂停和恢复功能。
 * - **资源管理**: 使用 RAII 模式和禁止拷贝确保线程资源的正确管理。
 * - **线程安全**: 使用互斥锁和原子变量保证内部状态和操作的线程安全。
 * - **扩展接口**: 包含未来集成线程池和线程间通信的占位接口。
 *
 * ### 使用示例
 *
 * @code
 * #include "ThreadWrapper.h"
 * #include <iostream>
 * #include <chrono>
 * #include <thread>
 *
 * // 示例任务函数
 * void my_task(int id, LSX_LIB::Thread::ThreadWrapper* self) {
 * std::cout << "任务 " << id << " 在线程 " << std::this_thread::get_id() << " 中启动。" << std::endl;
 *
 * // 任务主循环，检查停止和暂停标志
 * while (!self->stopFlag_.load()) {
 * if (self->pauseFlag_.load()) {
 * // 线程暂停，等待恢复通知
 * std::unique_lock<std::mutex> lk(self->mtx_);
 * self->cv_.wait(lk, [&]{ return !self->pauseFlag_.load() || self->stopFlag_.load(); });
 * if (self->stopFlag_.load()) break; // 如果在等待时收到停止信号，退出循环
 * std::cout << "任务 " << id << " 在线程 " << std::this_thread::get_id() << " 中恢复。" << std::endl;
 * }
 *
 * // 执行实际任务逻辑
 * std::cout << "任务 " << id << " 正在运行..." << std::endl;
 * std::this_thread::sleep_for(std::chrono::milliseconds(500)); // 模拟工作
 *
 * // 可以在这里添加任务完成条件，然后设置 stopFlag_ = true; break;
 * }
 *
 * std::cout << "任务 " << id << " 在线程 " << std::this_thread::get_id() << " 中停止。" << std::endl;
 * }
 *
 * int main() {
 * LSX_LIB::Thread::ThreadWrapper thread1;
 *
 * // 绑定任务
 * thread1.setTask(my_task, 1, &thread1); // 将 my_task 和参数绑定到线程
 *
 * // 启动线程
 * thread1.start();
 * std::cout << "线程 1 已启动。" << std::endl;
 *
 * std::this_thread::sleep_for(std::chrono::seconds(2)); // 等待线程运行一段时间
 *
 * std::cout << "请求暂停线程 1..." << std::endl;
 * thread1.pause();
 * std::this_thread::sleep_for(std::chrono::seconds(2)); // 观察暂停效果
 *
 * std::cout << "请求恢复线程 1..." << std::endl;
 * thread1.resume();
 * std::this_thread::sleep_for(std::chrono::seconds(2)); // 观察恢复效果
 *
 * std::cout << "请求停止线程 1..." << std::endl;
 * thread1.stop(); // 停止并等待线程结束
 * std::cout << "线程 1 已停止。" << std::endl;
 *
 * // 示例：使用移动语义
 * LSX_LIB::Thread::ThreadWrapper thread2;
 * thread2.setTask(my_task, 2, &thread2);
 * thread2.start();
 * std::cout << "线程 2 已启动。" << std::endl;
 *
 * LSX_LIB::Thread::ThreadWrapper thread3 = std::move(thread2); // 移动 thread2 到 thread3
 * std::cout << "线程 2 已移动到线程 3。" << std::endl;
 * // thread2 现在处于无效状态
 *
 * std::this_thread::sleep_for(std::chrono::seconds(2));
 *
 * std::cout << "请求停止线程 3..." << std::endl;
 * thread3.stop(); // 停止 thread3 (原 thread2)
 * std::cout << "线程 3 已停止。" << std::endl;
 *
 * std::cout << "主线程退出。" << std::endl;
 * return 0;
 * }
 * @endcode
 *
 * ### 注意事项
 * - **任务实现**: 绑定的任务函数内部需要包含一个循环，并在循环中检查 `stopFlag_` 来响应停止请求，以及检查 `pauseFlag_` 和使用条件变量 `cv_` 来实现暂停/恢复。直接执行一次就结束的任务不需要这些控制逻辑。
 * - **线程安全**: `ThreadWrapper` 类内部通过互斥锁和原子变量保证了自身的线程安全。但是，如果绑定的任务函数访问共享资源，任务函数本身需要自行处理同步问题。
 * - **拷贝/移动**: 线程对象是不可拷贝的，因为线程句柄是唯一的资源。支持移动语义，允许转移线程的所有权。
 * - **分离线程**: 如果使用 `start(true)` 启动分离线程，主程序退出时不会等待该线程。分离线程的资源由系统管理。通常推荐使用可 join 的线程，并在需要时调用 `stop` 或 `join`。
 * - **占位接口**: `threadPool_` 和 `communicator_` 成员变量以及相关的 `set` 方法是占位符，当前 `ThreadWrapper` 的核心功能不依赖于它们。
 * - **异常处理**: 任务函数内部抛出的未捕获异常会导致程序终止。建议在任务函数内部捕获并处理异常。
 */

//
// Created by admin on 2025/5/7.
//

#ifndef THREADWRAPPER_H
#define THREADWRAPPER_H
#pragma once
// 包含全局错误锁头文件
#include "GlobalErrorMutex.h"
// 包含 LIBLSX::LockManager::LockGuard 头文件
#include "LockGuard.h"

#include <thread> // 包含 std::thread
#include <mutex> // 包含 std::mutex
#include <condition_variable> // 包含 std::condition_variable
#include <atomic> // 包含 std::atomic
#include <functional> // 包含 std::function, std::bind
#include <memory> // 包含 std::shared_ptr
#include "ThreadState.h" // 包含 ThreadState 枚举定义
#include "IThreadPool.h" // 包含 IThreadPool 接口定义 (占位)
#include "ICommunicator.h" // 包含 ICommunicator 接口定义 (占位)

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
         * @brief C++ 单线程封装类。
         * 封装 std::thread，提供线程的生命周期管理（启动、停止、暂停、恢复）和状态跟踪功能。
         * 支持绑定可调用对象作为线程任务，并使用原子标志和条件变量实现线程控制。
         */
        class ThreadWrapper
        {
        public:
            /**
             * @brief 构造函数。
             * 初始化 ThreadWrapper 对象，设置原子标志和状态的初始值。
             * 不创建或启动线程。
             */
            ThreadWrapper();

            /**
             * @brief 析构函数。
             * 在对象销毁时自动调用 stop()，确保线程被停止并 join，释放资源。
             */
            ~ThreadWrapper();

            // 禁止拷贝构造函数和赋值运算符，线程对象不可拷贝，因为线程句柄是唯一的资源。
            /**
             * @brief 禁用拷贝构造函数。
             */
            ThreadWrapper(const ThreadWrapper&) = delete;
            /**
             * @brief 禁用赋值运算符。
             */
            ThreadWrapper& operator=(const ThreadWrapper&) = delete;

            // 允许移动构造函数和移动赋值运算符，支持线程资源的转移。
            /**
             * @brief 默认移动构造函数。
             */
            ThreadWrapper(ThreadWrapper&&) = default;
            /**
             * @brief 默认移动赋值运算符。
             */
            ThreadWrapper& operator=(ThreadWrapper&&) = default;

            /**
             * @brief 绑定要在此线程中执行的任务。
             * 将一个可调用对象及其参数绑定到 ThreadWrapper 内部，作为线程启动后要执行的任务。
             * 只能在线程未运行时调用。
             *
             * @tparam F 可调用对象类型。
             * @tparam Args 可调用对象的参数类型。
             * @param f 要执行的可调用对象。
             * @param args 可调用对象的参数。
             */
            template <typename F, typename... Args>
            void setTask(F&& f, Args&&... args)
            {
                std::lock_guard<std::mutex> lk(mtx_); // 保护 task_ 和 state_ 的访问
                // 使用 std::bind 绑定函数和参数，std::forward 保持参数的左右值属性
                task_ = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
                // 设置状态为 INIT，表示任务已设置但线程未启动
                state_.store(ThreadState::INIT);
            }

            /**
             * @brief 启动线程并执行绑定的任务。
             * 创建并启动内部的 std::thread 对象，该线程将执行 threadFunc 方法。
             * 只能在线程状态为 INIT 或 STOPPED 时调用。
             *
             * @param detached 如果为 true，线程将分离（detach），主程序退出时不会等待该线程；否则默认可 join。默认为 false。
             */
            void start(bool detached = false);

            /**
             * @brief 请求停止线程并等待其退出 (join)。
             * 设置停止标志，通知线程退出其任务循环，然后等待线程完成执行并 join。
             * 如果线程已分离，此方法无效。
             */
            void stop();

            /**
             * @brief 暂停线程执行任务。
             * 设置暂停标志，通知线程进入暂停状态。任务函数内部需要检查此标志并使用条件变量等待恢复。
             */
            void pause();

            /**
             * @brief 恢复线程执行任务。
             * 清除暂停标志，并通知等待中的线程恢复执行。
             */
            void resume();

            /**
             * @brief 停止当前线程（如果正在运行）并重新启动。
             * 调用 stop() 停止并等待当前线程，然后调用 start() 启动一个新的线程执行绑定的任务。
             *
             * @param detached 如果为 true，新线程将分离；否则默认可 join。
             */
            void restart(bool detached = false);

            /**
             * @brief 获取当前线程状态。
             * 返回线程当前的运行状态。
             *
             * @return 线程状态枚举值。
             */
            ThreadState getState() const { return state_.load(); }

            // --- 扩展接口 ---

            /**
             * @brief 设置线程池实例 (用于未来扩展)。
             * 将一个 IThreadPool 实例的共享指针与此 ThreadWrapper 关联。
             * 当前 ThreadWrapper 的核心功能不直接使用此接口，用于未来集成到线程池管理框架。
             *
             * @param pool 线程池的共享指针。
             */
            void setThreadPool(std::shared_ptr<IThreadPool> pool) { threadPool_ = pool; }

            /**
             * @brief 设置线程间通信实例 (用于未来扩展)。
             * 将一个 ICommunicator 实例的共享指针与此 ThreadWrapper 关联。
             * 当前 ThreadWrapper 的核心功能不直接使用此接口，用于未来集成线程间通信功能。
             *
             * @param comm 通信器的共享指针。
             */
            void setCommunicator(std::shared_ptr<ICommunicator> comm) { communicator_ = comm; }

        private:
            /**
             * @brief 线程实际执行函数。
             * 这是 std::thread 启动后执行的函数。它负责调用绑定的任务 (`task_`)，
             * 并在任务执行过程中处理暂停/恢复和停止的逻辑（如果任务函数支持）。
             * 还会更新线程状态 (`state_`)。
             */
            void threadFunc();

            /**
             * @brief 工作线程对象。
             * 封装了底层的 std::thread。
             */
            std::thread worker_;

            /**
             * @brief 要执行的任务。
             * 存储通过 setTask 绑定的可调用对象。
             */
            std::function<void()> task_;

            /**
             * @brief 互斥锁。
             * 用于保护 task_ 的访问以及与条件变量 cv_ 配合使用。
             */
            mutable std::mutex mtx_;

            /**
             * @brief 条件变量。
             * 用于实现线程的暂停/恢复机制，以及在 task_ 未设置时等待任务绑定（如果需要）。
             */
            std::condition_variable cv_;

            /**
             * @brief 停止标志。
             * 原子布尔变量，用于向线程发出停止请求。任务函数应定期检查此标志。
             */
            std::atomic<bool> stopFlag_;

            /**
             * @brief 暂停标志。
             * 原子布尔变量，用于控制线程的暂停和恢复。任务函数应检查此标志并在暂停时等待条件变量。
             */
            std::atomic<bool> pauseFlag_;

            /**
             * @brief 线程状态。
             * 原子枚举变量，用于跟踪线程的当前状态 (INIT, RUNNING, PAUSED, STOPPED)。
             */
            std::atomic<ThreadState> state_;

            /**
             * @brief 线程池接口 (占位)。
             * 用于未来将 ThreadWrapper 集成到线程池中，当前不直接使用。
             */
            std::shared_ptr<IThreadPool> threadPool_;

            /**
             * @brief 通信接口 (占位)。
             * 用于未来在线程间进行消息通信，当前不直接使用。
             */
            std::shared_ptr<ICommunicator> communicator_;
        };
    } // namespace LSX_LIB::Thread
} // namespace LSX_LIB

#endif //THREADWRAPPER_H
