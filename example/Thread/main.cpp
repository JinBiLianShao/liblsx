#include "ThreadWrapper.h"
#include "Scheduler.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <cstdlib> // 用于 std::exit

int main()
{
    // 使用新的命名空间
    using namespace LSX_LIB::Thread;

    std::cout << "--- LSX_LIB::Thread Example ---" << std::endl;

    // 示例 1: 直接管理单线程 (ThreadWrapper)
    std::cout << "\nExample 1: ThreadWrapper basic usage" << std::endl;
    ThreadWrapper tw;
    tw.setTask([]()
    {
        std::cout << "Hello from ThreadWrapper task\n";
        // 实际任务可能需要更长时间，例如 sleep 或执行复杂计算
        // std::this_thread::sleep_for(std::chrono::milliseconds(100));
    });
    std::cout << "Thread state after setTask: " << static_cast<int>(tw.getState()) << std::endl; // 0 = INIT
    tw.start();
    std::cout << "Thread state after start: " << static_cast<int>(tw.getState()) << std::endl; // 1 = RUNNING
    // 给予任务一点时间运行
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    tw.stop(); // 停止并等待线程退出
    std::cout << "Thread state after stop: " << static_cast<int>(tw.getState()) << std::endl; // 3 = STOPPED

    // 示例 2: 使用 Scheduler 进行延迟任务
    std::cout << "\nExample 2: Scheduler scheduleOnce" << std::endl;
    Scheduler sched_once;
    sched_once.scheduleOnce(1000, []()
    {
        // 1000ms (1秒) 后执行
        std::cout << "One-shot task executed after 1 second.\n";
    });
    std::cout << "Scheduled a one-shot task.\n";
    // 主线程需要等待足够长时间，让调度器有机会运行任务
    std::this_thread::sleep_for(std::chrono::seconds(2));
    sched_once.shutdown(); // 关闭调度器，停止并清理任务线程

    // 示例 3: 使用 Scheduler 进行周期性任务
    std::cout << "\nExample 3: Scheduler schedulePeriodic" << std::endl;
    Scheduler sched_periodic;
    // 每 500ms 执行一次任务
    sched_periodic.schedulePeriodic(500, []()
    {
        static int cnt = 0; // 静态变量在 lambda 的多次调用间保持状态
        std::cout << "Periodic task tick " << ++cnt << " (every 500ms).\n";
        if (cnt >= 5)
        {
            std::cout << "Periodic task finished after 5 ticks. Exiting example...\n";
            // 在示例中，达到条件后直接退出整个程序
            // 在实际应用中，更常见的做法是通知 Scheduler 停止或设置标志位
            std::exit(0); // 退出程序
        }
    });
    std::cout << "Scheduled a periodic task every 500ms.\n";

    // 主线程等待，让周期任务有机会运行
    // 注意：由于周期任务中调用了 std::exit(0)，这里可能不会运行到 sleep 结束
    std::this_thread::sleep_for(std::chrono::seconds(10));

    // 如果程序没有通过 std::exit 退出，这里将执行 shutdown
    std::cout << "\nExample finished. Shutting down sched_periodic...\n";
    sched_periodic.shutdown(); // 关闭调度器

    std::cout << "--- Example End ---" << std::endl;

    return 0;
}
