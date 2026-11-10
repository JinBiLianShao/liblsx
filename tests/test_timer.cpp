/**
 * @file test_timer.cpp
 * @brief Timer 模块深度测试：TimerWheel 一次性/周期/取消/并发、Stopwatch、Deadline
 */
#include "test_framework.h"

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

#include "Timer/Timer.h"

using namespace LSX_LIB;
using namespace std::chrono_literals;

LSX_TEST(TimerWheel, StartStopIdempotent) {
    Timer::TimerWheel timers;
    timers.start();
    timers.start();  // 幂等
    LSX_EXPECT_TRUE(timers.isRunning());
    timers.shutdown();
    timers.shutdown();  // 幂等
    LSX_EXPECT_TRUE(!timers.isRunning());
}

LSX_TEST(TimerWheel, OnceFiresAfterDelay) {
    Timer::TimerWheel timers;
    timers.start();
    std::atomic<bool> fired{false};
    auto id = timers.scheduleOnce(80, [&fired]() { fired = true; });
    LSX_EXPECT_TRUE(id.isValid());
    LSX_EXPECT_TRUE(timers.isActive(id));
    std::this_thread::sleep_for(250ms);
    LSX_EXPECT_TRUE(fired.load());
    LSX_EXPECT_TRUE(!timers.isActive(id));  // 触发后不再活跃
    timers.shutdown();
}

LSX_TEST(TimerWheel, OnceDoesNotFireBeforeDeadline) {
    Timer::TimerWheel timers;
    timers.start();
    std::atomic<bool> fired{false};
    timers.scheduleOnce(400, [&fired]() { fired = true; });
    std::this_thread::sleep_for(150ms);
    LSX_EXPECT_TRUE(!fired.load());  // 未到期不触发
    timers.shutdown();
}

LSX_TEST(TimerWheel, PeriodicFiresRepeatedly) {
    Timer::TimerWheel timers;
    timers.start();
    std::atomic<int> ticks{0};
    auto id = timers.schedulePeriodic(40, [&ticks]() { ++ticks; });
    std::this_thread::sleep_for(350ms);
    LSX_EXPECT_TRUE(ticks.load() >= 5);
    LSX_EXPECT_TRUE(ticks.load() <= 15);  // 不失控
    timers.cancel(id);
    int frozen = ticks.load();
    std::this_thread::sleep_for(120ms);
    LSX_EXPECT_EQ(frozen, ticks.load());  // 取消后不再增长
    timers.shutdown();
}

LSX_TEST(TimerWheel, CancelBeforeFirePreventsTrigger) {
    Timer::TimerWheel timers;
    timers.start();
    std::atomic<bool> fired{false};
    auto id = timers.scheduleOnce(200, [&fired]() { fired = true; });
    LSX_EXPECT_TRUE(timers.cancel(id));       // 触发前取消成功
    LSX_EXPECT_TRUE(!timers.cancel(id));      // 重复取消失败
    std::this_thread::sleep_for(300ms);
    LSX_EXPECT_TRUE(!fired.load());
    timers.shutdown();
}

LSX_TEST(TimerWheel, CancelFromInsideCallback) {
    Timer::TimerWheel timers;
    timers.start();
    std::atomic<int> ticks{0};
    Timer::TimerId self;
    self = timers.schedulePeriodic(30, [&]() {
        ++ticks;
        if (ticks.load() >= 3)
        {
            timers.cancel(self);  // 回调里取消自己（v0.1.2 设计保证支持）
        }
    });
    std::this_thread::sleep_for(300ms);
    LSX_EXPECT_TRUE(ticks.load() >= 3);
    LSX_EXPECT_TRUE(ticks.load() <= 6);  // 取消后停止
    timers.shutdown();
}

LSX_TEST(TimerWheel, ScheduleFromInsideCallback) {
    Timer::TimerWheel timers;
    timers.start();
    std::atomic<bool> second_fired{false};
    timers.scheduleOnce(30, [&]() {
        timers.scheduleOnce(30, [&second_fired]() { second_fired = true; });
    });
    std::this_thread::sleep_for(300ms);
    LSX_EXPECT_TRUE(second_fired.load());  // 回调内可再次调度
    timers.shutdown();
}

LSX_TEST(TimerWheel, ManyTimersSingleThread) {
    Timer::TimerWheel timers;
    timers.start();
    std::atomic<int> fired{0};
    std::vector<Timer::TimerId> ids;
    for (int i = 0; i < 200; ++i)
    {
        ids.push_back(timers.scheduleOnce(50 + (i % 100), [&fired]() { ++fired; }));
    }
    LSX_EXPECT_EQ(timers.size(), 200u);  // 200 个定时器
    std::this_thread::sleep_for(400ms);
    LSX_EXPECT_EQ(fired.load(), 200);    // 全部触发
    timers.shutdown();
}

LSX_TEST(TimerWheel, ShutdownCancelsPending) {
    Timer::TimerWheel timers;
    timers.start();
    std::atomic<bool> fired{false};
    timers.scheduleOnce(300, [&fired]() { fired = true; });
    timers.shutdown();  // shutdown 清空未触发定时器
    std::this_thread::sleep_for(400ms);
    LSX_EXPECT_TRUE(!fired.load());
}

LSX_TEST(TimerWheel, ScheduleAfterShutdownReturnsInvalid) {
    Timer::TimerWheel timers;
    timers.start();
    timers.shutdown();
    auto id = timers.scheduleOnce(50, [] {});
    LSX_EXPECT_TRUE(!id.isValid());  // shutdown 后拒绝新定时器
}

LSX_TEST(TimerWheel, DestructorsShutsDownAutomatically) {
    std::atomic<bool> fired{false};
    {
        Timer::TimerWheel timers;
        timers.start();
        timers.schedulePeriodic(20, [&fired]() { fired = true; });
        // 不调用 shutdown，直接析构
    }
    LSX_EXPECT_TRUE(true);  // 析构不挂起即通过
}

LSX_TEST(TimerWheel, MultipleWheelsIndependent) {
    Timer::TimerWheel t1, t2;
    t1.start();
    t2.start();
    std::atomic<int> a{0}, b{0};
    t1.schedulePeriodic(30, [&a]() { ++a; });
    t2.schedulePeriodic(45, [&b]() { ++b; });
    std::this_thread::sleep_for(300ms);
    t1.shutdown();
    t2.shutdown();
    LSX_EXPECT_TRUE(a.load() >= 3 && b.load() >= 2);
}

LSX_TEST(TimerWheel, InvalidParamsRejected) {
    Timer::TimerWheel timers;
    timers.start();
    LSX_EXPECT_TRUE(!timers.scheduleOnce(-1, [] {}).isValid());
    LSX_EXPECT_TRUE(!timers.schedulePeriodic(0, [] {}).isValid());
    LSX_EXPECT_TRUE(!timers.schedulePeriodic(-5, [] {}).isValid());
    LSX_EXPECT_TRUE(!timers.scheduleOnce(50, nullptr).isValid());
    timers.shutdown();
}

// ---------- Stopwatch ----------

LSX_TEST(Stopwatch, MeasuresElapsedMs) {
    Timer::Stopwatch sw;
    std::this_thread::sleep_for(60ms);
    std::int64_t ms = sw.elapsedMs();
    LSX_EXPECT_TRUE(ms >= 55 && ms <= 200);  // 留出调度余量
    sw.restart();
    LSX_EXPECT_TRUE(sw.elapsedMs() < 50);    // 重置后归零
}

LSX_TEST(Stopwatch, ElapsedSecondsIsConsistent) {
    Timer::Stopwatch sw;
    std::this_thread::sleep_for(50ms);
    double sec = sw.elapsedSeconds();
    LSX_EXPECT_TRUE(sec > 0.03 && sec < 0.2);
    LSX_EXPECT_NEAR(sec * 1000.0, static_cast<double>(sw.elapsedMs()), 5.0);
}

// ---------- Deadline ----------

LSX_TEST(Deadline, ExpiredAfterTimeout) {
    Timer::Deadline d(80);
    LSX_EXPECT_TRUE(!d.expired());
    std::this_thread::sleep_for(120ms);
    LSX_EXPECT_TRUE(d.expired());
    LSX_EXPECT_EQ(d.remainingMs(), 0);
}

LSX_TEST(Deadline, NeverExpiresWithZero) {
    Timer::Deadline d(0);  // <=0 表示永不超时
    LSX_EXPECT_TRUE(!d.expired());
    LSX_EXPECT_EQ(d.remainingMs(), -1);
    std::this_thread::sleep_for(30ms);
    LSX_EXPECT_TRUE(!d.expired());  // 仍然不超时
}

LSX_TEST(Deadline, RemainingCountsDown) {
    Timer::Deadline d(500);
    std::int64_t r1 = d.remainingMs();
    std::this_thread::sleep_for(60ms);
    std::int64_t r2 = d.remainingMs();
    LSX_EXPECT_TRUE(r1 > r2);
    LSX_EXPECT_TRUE(r1 <= 500 && r2 >= 0);
}
