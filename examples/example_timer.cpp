/**
 * @file example_timer.cpp
 * @brief LIBLSX Timer 模块使用示例
 *
 * 演示内容：
 *   1. TimerWheel —— 单线程驱动的多定时器（一次性 + 周期 + 取消）
 *   2. Stopwatch  —— 高精度计时
 *   3. Deadline   —— 超时判断与剩余时间
 *
 * 关键点：
 *   - TimerWheel 只用 1 个工作线程驱动任意数量定时器，适合大规模定时场景；
 *     回调在定时器线程串行执行，回调内不要做阻塞操作。
 *   - 需要「暂停/恢复」控制时改用 Thread 模块的 Scheduler。
 */

#include "Timer/Timer.h"

#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>

namespace tmr = LSX_LIB::Timer;   // 别名避开 C 标准库的 struct tm

// ---------------------------------------------------------------------------
// 演示 1：TimerWheel 多定时器
// ---------------------------------------------------------------------------
static void demo_timer_wheel()
{
    std::cout << "\n===== 演示 1：TimerWheel 定时器 =====\n";

    tmr::TimerWheel wheel;
    wheel.start();                    // 启动工作线程（幂等）

    // 一次性：300ms 后触发一次
    wheel.scheduleOnce(300, []() {
        std::cout << "  [once] 300ms 到，触发一次\n";
    });

    // 周期性：每 150ms 触发一次，计数 4 次后自我取消
    std::atomic<int> count{0};
    tmr::TimerId periodic_id;
    periodic_id = wheel.schedulePeriodic(150, [&]() {
        std::cout << "  [periodic] 第 " << ++count << " 次\n";
        if (count.load() >= 4) {
            wheel.cancel(periodic_id);   // 回调内取消自己：安全
        }
    });

    // 等待周期任务自取消 + 一次性任务完成
    std::this_thread::sleep_for(std::chrono::milliseconds(900));

    // isActive/size 查询
    std::cout << "  周期定时器仍活跃: "
              << (wheel.isActive(periodic_id) ? "是" : "否") << "\n";
    std::cout << "  待触发定时器数: " << wheel.size() << "\n";

    wheel.shutdown();                 // 停止线程（析构也会调用）
}

// ---------------------------------------------------------------------------
// 演示 2：Stopwatch 计时
// ---------------------------------------------------------------------------
static void demo_stopwatch()
{
    std::cout << "\n===== 演示 2：Stopwatch 计时 =====\n";

    tmr::Stopwatch sw;
    std::this_thread::sleep_for(std::chrono::milliseconds(120));
    std::cout << "  已流逝 " << sw.elapsedMs() << " ms（整数毫秒）\n";
    std::cout << "  已流逝 " << sw.elapsedSeconds() << " s（浮点秒）\n";

    sw.restart();                     // 重新计时
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    std::cout << "  重启后又过了 " << sw.elapsedUs() << " us\n";
}

// ---------------------------------------------------------------------------
// 演示 3：Deadline 超时判断
// ---------------------------------------------------------------------------
static void demo_deadline()
{
    std::cout << "\n===== 演示 3：Deadline 超时判断 =====\n";

    tmr::Deadline dl(100);             // 100ms 期限
    std::cout << "  剩余 " << dl.remainingMs() << " ms，已超时: "
              << (dl.expired() ? "是" : "否") << "\n";

    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    std::cout << "  150ms 后剩余 " << dl.remainingMs() << " ms，已超时: "
              << (dl.expired() ? "是" : "否") << "\n";

    tmr::Deadline never(0);           // timeout_ms <= 0 表示永不超时
    std::cout << "  永不超时：expired=" << (never.expired() ? "是" : "否")
              << "，remainingMs=" << never.remainingMs() << "（-1 表示永不）\n";
}

int main()
{
    demo_timer_wheel();
    demo_stopwatch();
    demo_deadline();
    return 0;
}
