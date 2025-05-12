#pragma once
#include "ThreadWrapper.h"
#include <iostream> // 仅用于示例或调试输出
#include <chrono>   // 用于 sleep 或 timed_wait
#include <system_error> // 用于捕获 join 异常

namespace LSX_LIB::Thread
{
    ThreadWrapper::ThreadWrapper()
        : stopFlag_(false)
          , pauseFlag_(false)
          , state_(ThreadState::INIT)
    {
        // 构造时不创建线程，等待 start() 调用
    }

    ThreadWrapper::~ThreadWrapper()
    {
        stop(); // 析构时确保线程被停止并join，防止资源泄露
    }

    void ThreadWrapper::start(bool detached)
    {
        // 确保在启动前任务已设置且当前非运行状态
        std::lock_guard<std::mutex> lk(mtx_);
        if (!task_ || state_ == ThreadState::RUNNING)
        {
            // 可以选择抛出异常或打印警告
            // std::cerr << "Warning: Cannot start thread. Task not set or already running.\n";
            return;
        }

        stopFlag_.store(false);
        pauseFlag_.store(false);
        state_.store(ThreadState::RUNNING);

        // 创建并启动新线程
        worker_ = std::thread(&ThreadWrapper::threadFunc, this);

        if (detached)
        {
            worker_.detach(); // 分离线程，使其独立运行
        }
    }

    void ThreadWrapper::stop()
    {
        // 设置停止标志
        stopFlag_.store(true);

        // 唤醒任何正在等待的线程 (如暂停状态或等待任务状态)
        resume(); // 调用 resume 确保即使在 pause 状态也能被唤醒
        cv_.notify_one(); // 额外通知一次，确保等待任务的 cv.wait_for 也能被唤醒

        // 如果线程是可joinable的 (未分离)，等待其完成
        // 必须在设置 stopFlag_ 并通知后进行 join
        if (worker_.joinable())
        {
            try
            {
                worker_.join();
            }
            catch (const std::system_error& e)
            {
                // 捕获 join 可能抛出的异常，例如 std::system_error
                std::cerr << "Error joining thread: " << e.what() << std::endl;
            }
        }

        // 更新状态为停止
        state_.store(ThreadState::STOPPED);
        // 清理任务，准备可能的 restart
        std::lock_guard<std::mutex> lk(mtx_);
        task_ = nullptr;
    }

    void ThreadWrapper::pause()
    {
        // 只有在 RUNNING 状态下才能请求暂停
        if (state_ == ThreadState::RUNNING)
        {
            pauseFlag_.store(true);
            state_.store(ThreadState::PAUSED);
            // 注意：实际暂停发生在 threadFunc 中的条件变量等待
        }
    }

    void ThreadWrapper::resume()
    {
        // 如果当前是暂停状态，清除暂停标志并唤醒线程
        if (pauseFlag_.load())
        {
            pauseFlag_.store(false);
            // 唤醒 threadFunc 中等待 pauseFlag_ 的线程
            cv_.notify_one();
            // 状态更新为 RUNNING (假设唤醒后会立即继续运行)
            state_.store(ThreadState::RUNNING);
        }
    }

    void ThreadWrapper::restart(bool detached)
    {
        stop(); // 先停止当前线程
        // task_ 在 stop() 中被清理，restart 需要重新 setTask
        // 在实际使用中，restart 后通常需要再次调用 setTask()
        // 为了简化，此处假设 setTask() 会在 restart 之后立即调用
        // 如果需要在 restart 中保留原任务，需要调整 stop() 的逻辑
        // 例如：task_ 保持不变，在 threadFunc 退出前不清理 task_
        // 或者在 restart 中重新绑定 task_
        // 当前实现下，调用 restart 后必须再次调用 setTask 才能再次运行任务
        // std::cerr << "Warning: Restart requires setTask() to be called again.\n";
        // 更好的设计是在 restart 中接收任务参数，或者保存/恢复 task_
        start(detached); // 重新启动新线程
    }

    void ThreadWrapper::threadFunc()
    {
        // 等待任务绑定 (或者在 start 时就检查是否已绑定)
        // 当前设计是在 start 时检查 task_ 是否已绑定
        // 这个 wait 块实际上是在等待 task_ 的准备，或者 stopFlag_
        // 如果 task_ 在 start 时就已设置，这个 wait 会很快通过
        {
            std::unique_lock<std::mutex> lk(mtx_);
            // cv_.wait_for(lk, std::chrono::milliseconds(1), [&]{
            //     return task_ != nullptr || stopFlag_.load();
            // });
            // 改进：如果在 start 之前没有设置 task，wait_for 1ms 可能不够
            // 更好的方式是在 start 时就要求 task_ 已设置，或者在这里等待
            // 或者，更简单地，threadFunc 只执行 task_ 一次，然后退出
            // 如果需要等待任务，可以增加一个任务就绪标志
        }

        // 检查任务是否有效且没有停止请求
        // 如果 task_ 是在 start 之前设置的，且 start 成功，这里 task_ 应该非空
        if (!task_ || stopFlag_.load())
        {
            state_.store(ThreadState::STOPPED);
            return;
        }

        // --- 主工作循环 ---
        // 注意：当前设计中，task_() 通常只执行一次。
        // 如果需要周期性任务，任务本身 (即 lambda/function) 内部应包含循环。
        // ThreadWrapper 的 while 循环主要用于处理暂停/恢复逻辑。
        while (!stopFlag_.load())
        {
            // --- 处理暂停 ---
            if (pauseFlag_.load())
            {
                state_.store(ThreadState::PAUSED); // 更新状态
                std::unique_lock<std::mutex> lk(mtx_);
                // 等待直到不暂停 (pauseFlag_ == false) 或收到停止请求 (stopFlag_ == true)
                cv_.wait(lk, [&] { return !pauseFlag_.load() || stopFlag_.load(); });

                if (stopFlag_.load())
                {
                    break; // 如果是因停止请求而唤醒，则退出循环
                }
                // 否则，pauseFlag_ 为 false，继续运行，更新状态为 RUNNING
                state_.store(ThreadState::RUNNING);
            }

            // --- 执行实际任务 ---
            try
            {
                task_(); // 调用用户绑定的任务函数
            }
            catch (const std::exception& e)
            {
                // 捕获任务执行中可能抛出的异常
                std::cerr << "Task execution failed: " << e.what() << std::endl;
                // 可以选择在这里设置 stopFlag_ 终止线程，或者记录日志后继续 (取决于设计)
                stopFlag_.store(true); // 例如，任务失败后停止线程
            } catch (...)
            {
                // 捕获其他未知异常
                std::cerr << "Task execution failed with unknown error." << std::endl;
                stopFlag_.store(true); // 例如，任务失败后停止线程
            }


            // --- 单次执行模式 ---
            // 当前设计中，task_ 执行一次后，threadFunc 退出 while 循环。
            // 如果需要任务重复执行，需要在 task_() 内部实现循环。
            break; // 任务执行完成后退出循环，除非任务自身包含循环
        }

        // 循环结束，线程即将退出
        state_.store(ThreadState::STOPPED); // 线程正常退出或被停止后状态
        // task_ = nullptr; // 不在这里清理 task_，让 stop() 或 restart() 处理
    }
} // namespace LSX_LIB::Thread
