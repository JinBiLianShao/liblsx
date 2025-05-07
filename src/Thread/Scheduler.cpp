#include "Scheduler.h"
#include <chrono>
#include <iostream> // 用于可能的错误/调试输出
#include <algorithm> // 用于清理已完成的任务
#include <vector>    // 显式包含 vector

namespace LSX_LIB::Thread
{
    void Scheduler::scheduleOnce(int delayMs, std::function<void()> func)
    {
        // 创建一个 ThreadWrapper 来执行一次性延迟任务
        auto tw = std::make_shared<ThreadWrapper>();

        // 绑定任务：先等待指定时间，然后执行实际任务
        // 捕获 tw 智能指针，以延长其生命周期至 lambda 执行完毕
        tw->setTask([delayMs, func, tw]()
        {
            if (delayMs > 0)
            {
                // 使用 sleep_for 模拟延迟
                // 注意：如果需要更精确的延迟且能响应停止，应使用条件变量的 timed_wait
                std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
            }
            // 在执行实际任务前检查停止标志
            if (tw->getState() != ThreadState::STOPPED)
            {
                try
                {
                    func(); // 执行用户任务
                }
                catch (const std::exception& e)
                {
                    std::cerr << "Scheduled task execution failed: " << e.what() << std::endl;
                } catch (...)
                {
                    std::cerr << "Scheduled task execution failed with unknown error." << std::endl;
                }
            }
            // 任务执行完毕（或被停止），可以在 Scheduler 中清理
            // 这里不直接清理，由 shutdown 或定期清理机制处理已停止的任务
        });

        // 启动线程 (分离，因为我们不关心等待单次任务完成，并希望其完成后自动清理资源)
        // 如果希望能在 shutdown 时等待所有一次性任务完成，这里不应分离
        tw->start(true); // 使用分离线程

        // 将线程包装器添加到列表中管理
        // 注意：对于分离线程，tasks_ 列表中的 shared_ptr 只是一个引用
        // 线程结束后，ThreadWrapper 对象会自动销毁（如果tw是最后一个引用）
        // 但 tasks_ 列表中的 shared_ptr 不会自动移除，需要定期清理
        std::lock_guard<std::mutex> lk(tasks_mtx_);
        tasks_.push_back(tw);

        // 考虑定期清理 tasks_ 中已经 STOPPED 状态且已分离的 ThreadWrapper
        // 否则 tasks_ 会一直增长
        // 例如，可以在每次添加任务后，或者在 shutdown 中添加清理逻辑
        // tasks_.erase(std::remove_if(tasks_.begin(), tasks_.end(),
        //                              [](const std::shared_ptr<ThreadWrapper>& t){
        //                                  return t && t->getState() == ThreadState::STOPPED;
        //                              }), tasks_.end());
    }

    void Scheduler::schedulePeriodic(int intervalMs, std::function<void()> func)
    {
        // 创建一个 ThreadWrapper 来执行周期性任务
        auto tw = std::make_shared<ThreadWrapper>();

        // 绑定任务：在无限循环中执行任务并等待指定间隔
        // 注意：退出循环的条件是 ThreadWrapper 接收到停止请求
        // 捕获 tw 智能指针，以延长其生命周期
        tw->setTask([intervalMs, func, tw]()
        {
            // 周期任务的循环是在这里实现的
            while (tw->getState() != ThreadState::STOPPED)
            {
                // 在执行任务前检查是否被暂停
                // ThreadWrapper 的 threadFunc 会在 task_() 调用前处理暂停
                // 所以这里不需要额外的暂停检查，调用 func() 即可

                try
                {
                    func(); // 执行用户任务
                }
                catch (const std::exception& e)
                {
                    std::cerr << "Periodic task execution failed: " << e.what() << std::endl;
                    // 周期任务失败是否停止取决于需求，这里选择继续
                } catch (...)
                {
                    std::cerr << "Periodic task execution failed with unknown error." << std::endl;
                    // 选择继续
                }

                // 执行完任务后等待指定间隔
                // 在等待期间也需要检查停止标志
                // std::this_thread::sleep_for 是阻塞的，如果等待期间需要响应停止，
                // 更好的方式是使用条件变量进行 timed_wait 并检查停止标志
                auto start_wait = std::chrono::steady_clock::now();
                auto end_wait = start_wait + std::chrono::milliseconds(intervalMs);
                bool stopped_during_wait = false;
                while (std::chrono::steady_clock::now() < end_wait)
                {
                    if (tw->getState() == ThreadState::STOPPED)
                    {
                        stopped_during_wait = true;
                        break;
                    }
                    // 短暂休眠以避免忙等
                    std::this_thread::sleep_for(std::chrono::milliseconds(10)); // 或更短
                }
                if (stopped_during_wait) break; // 如果等待期间停止，退出周期循环
            }
            // 周期任务结束（因停止请求）
        });

        // 启动线程 (默认不分离，shutdown 需要 join 以确保线程退出)
        tw->start(false); // 使用可join线程

        // 将线程包装器添加到列表中管理
        std::lock_guard<std::mutex> lk(tasks_mtx_);
        tasks_.push_back(tw);

        // 对于可join线程，shutdown 时会清理
        // 对于分离线程（如 scheduleOnce 使用），需要定期清理 tasks_
    }

    void Scheduler::shutdown()
    {
        // 锁定 tasks_ 向量，防止在停止时有新任务被添加
        std::lock_guard<std::mutex> lk(tasks_mtx_);

        // 遍历所有任务，请求停止并join (对于可join的线程)
        // 对于分离的线程，stop() 只是设置标志，join 会失败或无操作
        for (auto& tw : tasks_)
        {
            // 检查智能指针是否有效
            if (tw)
            {
                tw->stop(); // 请求线程停止并等待其join完成 (如果可join)
            }
        }

        // 清空任务列表
        tasks_.clear();
    }
}
