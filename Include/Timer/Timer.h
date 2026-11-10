/**
 * @file Timer.h
 * @brief LIBLSX 工具库 - 定时器模块
 * @details 提供 LSX_LIB::Timer 命名空间下的定时能力：
 *  - TimerWheel：单工作线程驱动的多定时器管理（一次性 + 周期），支持取消；
 *    相比 Scheduler 的"一任务一线程"模型，成百上千个定时器只占一个线程。
 *  - TimerId：定时器句柄，用于取消与状态查询。
 *  - Stopwatch：基于 steady_clock 的计时工具。
 *  - Deadline：超时判断工具。
 * @author 连思鑫（jinbilianshao）
 * @date 2026-11-10
 * @version 1.0
 *
 * ### 线程安全
 * 所有公开方法均可多线程调用；回调在定时器工作线程中执行，
 * 回调内部不应执行阻塞操作（会拖延同轮其他定时器）。
 *
 * ### 使用示例
 * @code
 * #include "Timer/Timer.h"
 *
 * LSX_LIB::Timer::TimerWheel timers;
 * timers.start();
 *
 * // 一次性：500ms 后触发
 * auto id1 = timers.scheduleOnce(500, []{ do_something(); });
 *
 * // 周期性：每 100ms 触发一次
 * auto id2 = timers.schedulePeriodic(100, []{ tick(); });
 *
 * // 取消
 * timers.cancel(id2);
 *
 * timers.shutdown();
 * @endcode
 */
#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_map>
#include <vector>

namespace LSX_LIB {
namespace Timer {

using Clock = std::chrono::steady_clock;
using TimePoint = Clock::time_point;

/// 定时器唯一句柄，cancel/isActive 以此为凭据
struct TimerId
{
    std::uint64_t value = 0;

    bool isValid() const { return value != 0; }
    bool operator==(const TimerId& o) const { return value == o.value; }
};

/// 供 unordered_map 以 TimerId 为键使用
struct TimerIdHash
{
    std::size_t operator()(const TimerId& id) const
    {
        return std::hash<std::uint64_t>{}(id.value);
    }
};

/**
 * @class TimerWheel
 * @brief 单线程驱动的多定时器管理器
 * @details 内部一个工作线程按最近到期时间睡眠，到期回调依次在同一线程执行。
 * 周期任务触发后自动重排下一次到期。shutdown 后不可复用（析构自动 shutdown）。
 */
class TimerWheel
{
public:
    using Callback = std::function<void()>;

    TimerWheel() = default;
    ~TimerWheel();

    TimerWheel(const TimerWheel&) = delete;
    TimerWheel& operator=(const TimerWheel&) = delete;

    /// 启动工作线程（幂等）
    void start();

    /// 停止工作线程并清空未触发的定时器（幂等；之后不可再 schedule）
    void shutdown();

    /// @brief 一次性定时器：delay_ms 后在定时器线程执行一次 func
    /// @return 定时器句柄；shutdown 后返回无效句柄
    TimerId scheduleOnce(std::int64_t delay_ms, Callback func);

    /// @brief 周期性定时器：每 interval_ms 执行一次，直至 cancel/shutdown
    /// @return 定时器句柄；shutdown 后返回无效句柄
    TimerId schedulePeriodic(std::int64_t interval_ms, Callback func);

    /// @brief 取消定时器；返回是否成功取消（false = 已触发/不存在/已 shutdown）
    bool cancel(TimerId id);

    /// @brief 定时器是否仍处于待触发状态
    bool isActive(TimerId id) const;

    /// @brief 当前待触发的定时器数量
    std::size_t size() const;

    /// @brief 工作线程是否在运行
    bool isRunning() const { return running_.load(); }

private:
    struct Entry
    {
        TimerId id;
        TimePoint deadline;
        std::int64_t interval_ms = 0;  // 0 = 一次性
        Callback func;
    };

    /// 最小堆比较：deadline 早者优先
    struct Later
    {
        bool operator()(const Entry& a, const Entry& b) const
        {
            return a.deadline > b.deadline;
        }
    };

    void workerLoop();
    TimerId insert(Entry entry);

    mutable std::mutex mtx_;
    std::condition_variable cv_;
    std::priority_queue<Entry, std::vector<Entry>, Later> queue_;
    std::unordered_map<std::uint64_t, bool> alive_;  // id -> 是否待触发
    std::uint64_t next_id_ = 1;
    std::thread worker_;
    std::atomic<bool> running_{false};
    std::atomic<bool> shutdown_done_{false};
};

/**
 * @class Stopwatch
 * @brief 基于 steady_clock 的计时工具
 */
class Stopwatch
{
public:
    Stopwatch() : start_(Clock::now()) {}

    /// 重新开始计时
    void restart() { start_ = Clock::now(); }

    /// @brief 已流逝毫秒数（整数）
    std::int64_t elapsedMs() const
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
                   Clock::now() - start_).count();
    }

    /// @brief 已流逝微秒数（整数）
    std::int64_t elapsedUs() const
    {
        return std::chrono::duration_cast<std::chrono::microseconds>(
                   Clock::now() - start_).count();
    }

    /// @brief 已流逝秒数（浮点）
    double elapsedSeconds() const
    {
        return std::chrono::duration<double>(Clock::now() - start_).count();
    }

private:
    TimePoint start_;
};

/**
 * @class Deadline
 * @brief 超时判断工具：以构造时刻为起点，判断是否超时及剩余时间
 */
class Deadline
{
public:
    explicit Deadline(std::int64_t timeout_ms)
        : timeout_ms_(timeout_ms), start_(Clock::now()) {}

    /// 是否已超时（timeout_ms <= 0 表示永不超时）
    bool expired() const
    {
        if (timeout_ms_ <= 0) return false;
        return elapsedMs() >= timeout_ms_;
    }

    /// 剩余毫秒数；永不超时时返回 -1
    std::int64_t remainingMs() const
    {
        if (timeout_ms_ <= 0) return -1;
        std::int64_t left = timeout_ms_ - elapsedMs();
        return left > 0 ? left : 0;
    }

private:
    std::int64_t elapsedMs() const
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
                   Clock::now() - start_).count();
    }

    std::int64_t timeout_ms_;
    TimePoint start_;
};

} // namespace Timer
} // namespace LSX_LIB
