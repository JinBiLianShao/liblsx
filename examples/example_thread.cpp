/**
 * @file example_thread.cpp
 * @brief LIBLSX Thread 模块使用示例
 *
 * 演示内容：
 *   1. ThreadWrapper —— 单线程封装：绑定任务、启动、暂停、恢复、停止、重启
 *   2. Scheduler    —— 任务调度器：一次性延迟任务 + 周期性任务
 *   3. ThreadPool   —— 线程池：提交带返回值/参数的任务，用 std::future 取结果
 *
 * 关键点：
 *   - ThreadWrapper 绑定的任务执行完就退出；周期任务要在任务函数内部循环，
 *     并配合外部标志响应停止（stop() 会 join，任务不退出则一直等）。
 *   - Scheduler 为每个任务开一个线程，适合"任务少、要暂停/恢复"；
 *     定时器数量多时用 Timer 模块的 TimerWheel（一个线程驱动所有定时器）。
 */

#include "Thread/ThreadWrapper.h"   // 单线程封装
#include "Thread/Scheduler.h"       // 任务调度器
#include "Thread/ThreadPool.h"      // 线程池

#include <atomic>
#include <chrono>
#include <future>
#include <iostream>
#include <string>
#include <thread>

namespace th = LSX_LIB::Thread;

// ---------------------------------------------------------------------------
// 演示 1：ThreadWrapper —— 单线程的生命周期管理
// ---------------------------------------------------------------------------
static void demo_thread_wrapper()
{
    std::cout << "\n===== 演示 1：ThreadWrapper 线程生命周期 =====\n";

    // 共享标志：任务循环据此退出；stop() 前先置位，保证任务能结束。
    std::atomic<bool> stop{false};
    std::atomic<int> counter{0};

    th::ThreadWrapper worker;

    // 绑定任务：周期任务必须自带循环，并在循环里检查停止标志。
    // ThreadWrapper 的 stopFlag_ 是私有成员，任务函数无法直接读到，
    // 所以这里用外部原子变量做协作式停止。
    worker.setTask([&stop, &counter]() {
        while (!stop.load()) {
            std::cout << "  [worker] 工作一次，计数 " << ++counter << "\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
    });

    worker.start();                       // 启动线程（默认可 join）
    std::this_thread::sleep_for(std::chrono::milliseconds(550));

    // pause/resume：ThreadWrapper 会在 threadFunc 的任务循环前后检查暂停标志，
    // 但任务自身若在 sleep 中，要等 sleep 结束才真正挂起——协作式语义。
    worker.pause();
    std::cout << "  [main] 已请求暂停\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(400));
    worker.resume();
    std::cout << "  [main] 已恢复\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    stop.store(true);                     // 先让任务循环退出
    worker.stop();                        // 再停止并 join，等线程真正结束
    std::cout << "  [main] 线程已停止，最终计数 " << counter.load() << "\n";
}

// ---------------------------------------------------------------------------
// 演示 2：Scheduler —— 一次性 + 周期性任务
// ---------------------------------------------------------------------------
static void demo_scheduler()
{
    std::cout << "\n===== 演示 2：Scheduler 任务调度 =====\n";

    th::Scheduler sched;

    // 一次性任务：延迟 200ms 执行一次，内部起一个独立线程。
    sched.scheduleOnce(200, []() {
        std::cout << "  [once] 200ms 后执行了一次\n";
    });

    // 周期任务：每 300ms 执行一次。任务函数无需自己写循环，
    // Scheduler 会反复调用，直到 shutdown()。
    sched.schedulePeriodic(300, []() {
        static int n = 0;
        std::cout << "  [periodic] 第 " << ++n << " 次触发\n";
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    sched.shutdown();                     // 停止所有任务并等待线程退出
    std::cout << "  [main] 调度器已关闭\n";
}

// ---------------------------------------------------------------------------
// 演示 3：ThreadPool —— 提交任务并取回结果
// ---------------------------------------------------------------------------
static void demo_thread_pool()
{
    std::cout << "\n===== 演示 3：ThreadPool 并发任务 =====\n";

    th::ThreadPool pool(4);               // 4 个工作线程

    // 提交带参数、带返回值的任务，返回 std::future
    auto f1 = pool.enqueue([](int a, int b) { return a + b; }, 3, 4);
    auto f2 = pool.enqueue([](const std::string& s) { return s.size(); },
                           std::string("hello"));

    // 无返回值任务走重载 enqueue(std::function<void()>)
    pool.enqueue([]() { std::cout << "  [pool] 一个无返回值的任务\n"; });

    std::cout << "  [main] 3 + 4 = " << f1.get() << "\n";
    std::cout << "  [main] len(hello) = " << f2.get() << "\n";

    pool.shutdown();                      // 显式关闭（析构也会自动调用）
}

int main()
{
    demo_thread_wrapper();
    demo_scheduler();
    demo_thread_pool();
    return 0;
}
