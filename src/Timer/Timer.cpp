/**
 * @file Timer.cpp
 * @brief Timer 模块实现
 */
#include "Timer/Timer.h"

#include <algorithm>

namespace LSX_LIB {
namespace Timer {

// ---------- TimerWheel ----------

TimerWheel::~TimerWheel()
{
    shutdown();
}

void TimerWheel::start()
{
    std::lock_guard<std::mutex> lk(mtx_);
    if (running_.load())
    {
        return;  // 幂等
    }
    shutdown_done_.store(false);
    running_.store(true);
    worker_ = std::thread(&TimerWheel::workerLoop, this);
    cv_.notify_one();
}

void TimerWheel::shutdown()
{
    if (shutdown_done_.exchange(true))
    {
        return;  // 已 shutdown
    }
    running_.store(false);
    {
        std::lock_guard<std::mutex> lk(mtx_);
        // 清空待触发定时器
        while (!queue_.empty())
        {
            queue_.pop();
        }
        alive_.clear();
    }
    cv_.notify_all();
    if (worker_.joinable())
    {
        worker_.join();
    }
}

TimerId TimerWheel::insert(Entry entry)
{
    std::lock_guard<std::mutex> lk(mtx_);
    if (!running_.load())
    {
        return TimerId{};  // shutdown 后不再接受新定时器
    }
    const TimerId id{next_id_++};
    entry.id = id;
    alive_[id.value] = true;
    queue_.push(std::move(entry));
    cv_.notify_one();  // 新任务可能比当前等待期限更早
    return id;
}

TimerId TimerWheel::scheduleOnce(std::int64_t delay_ms, Callback func)
{
    if (!func || delay_ms < 0)
    {
        return TimerId{};
    }
    Entry e;
    e.deadline = Clock::now() + std::chrono::milliseconds(delay_ms);
    e.interval_ms = 0;
    e.func = std::move(func);
    return insert(std::move(e));
}

TimerId TimerWheel::schedulePeriodic(std::int64_t interval_ms, Callback func)
{
    if (!func || interval_ms <= 0)
    {
        return TimerId{};
    }
    Entry e;
    e.deadline = Clock::now() + std::chrono::milliseconds(interval_ms);
    e.interval_ms = interval_ms;
    e.func = std::move(func);
    return insert(std::move(e));
}

bool TimerWheel::cancel(TimerId id)
{
    if (!id.isValid())
    {
        return false;
    }
    std::lock_guard<std::mutex> lk(mtx_);
    auto it = alive_.find(id.value);
    if (it == alive_.end() || !it->second)
    {
        return false;  // 已触发或不存在
    }
    alive_.erase(it);
    // 堆中对应 Entry 惰性清除：workerLoop 弹出时核对 alive_
    cv_.notify_one();
    return true;
}

bool TimerWheel::isActive(TimerId id) const
{
    if (!id.isValid())
    {
        return false;
    }
    std::lock_guard<std::mutex> lk(mtx_);
    auto it = alive_.find(id.value);
    return it != alive_.end() && it->second;
}

std::size_t TimerWheel::size() const
{
    std::lock_guard<std::mutex> lk(mtx_);
    return alive_.size();
}

void TimerWheel::workerLoop()
{
    std::unique_lock<std::mutex> lk(mtx_);
    while (running_.load())
    {
        if (queue_.empty())
        {
            // 无任务：挂起等待新任务或 shutdown
            cv_.wait(lk, [this] { return !running_.load() || !queue_.empty(); });
            continue;
        }

        const TimePoint next_deadline = queue_.top().deadline;
        // 等到最近到期时间，或有新任务/取消/shutdown 打断
        cv_.wait_until(lk, next_deadline, [this, next_deadline] {
            return !running_.load() ||
                   (!queue_.empty() && queue_.top().deadline < next_deadline);
        });

        if (!running_.load())
        {
            break;
        }

        const auto now = Clock::now();
        // 触发所有已到期且未被取消的定时器
        while (!queue_.empty() && queue_.top().deadline <= now)
        {
            Entry e = const_cast<Entry&&>(queue_.top());
            queue_.pop();

            auto it = alive_.find(e.id.value);
            if (it == alive_.end() || !it->second)
            {
                continue;  // 已取消，惰性跳过
            }

            if (e.interval_ms > 0)
            {
                // 周期任务：重新登记下一次到期（在释放锁执行回调之前完成，
                // 保证回调里 cancel 自己后不再重排）
                Entry next = e;
                next.deadline = now + std::chrono::milliseconds(e.interval_ms);
                queue_.push(std::move(next));
            }
            else
            {
                alive_.erase(it);
            }

            // 解锁执行回调（回调中允许调用 cancel/schedule 等公开方法）
            Callback func = std::move(e.func);
            lk.unlock();
            func();
            lk.lock();
        }
    }
}

} // namespace Timer
} // namespace LSX_LIB
