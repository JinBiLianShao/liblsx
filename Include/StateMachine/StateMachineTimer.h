/**
 * @file StateMachineTimer.h
 * @brief StateMachine 与 TimerWheel 的可选适配层
 * @details 主状态机头零依赖、无内部线程；本头提供把 addTimeout 挂到
 *          LSX_LIB::Timer::TimerWheel 的便捷适配，让状态机在真实工程中
 *          自动按时钟驱动，而无需在主循环里手动 tick()。
 *
 * 依赖：Timer/Timer.h（库内模块）。不引入额外第三方依赖。
 *
 * @author 连思鑫（jinbilianshao）
 * @date 2026-11-11
 * @version 1.0
 *
 * ### 使用示例
 * @code
 * LSX_LIB::Timer::TimerWheel timers;
 * timers.start();
 * LSX_LIB::StateMachine::StateMachine<S, E, Ctx> fsm(S::Idle, ctx);
 * LSX_LIB::StateMachine::TimerDriven<...> driven(fsm, timers);
 * driven.autoTimeout(S::Idle, 1000, E::Timeout);  // 进入 Idle 1s 后自动派发
 * @endcode
 */
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

#include "StateMachine.h"
#include "Timer/Timer.h"

namespace LSX_LIB {
namespace StateMachine {

/**
 * @class TimerDriven
 * @brief 把状态机的状态进入事件接到 TimerWheel，实现自动超时派发
 * @details 原理：用 onTransition 观察所有成功转换，进入新状态时取消旧定时器、
 *          按该状态注册的超时表启动新定时器；定时器回调里向状态机 dispatch。
 * @warning 定时器回调运行在 TimerWheel 工作线程，dispatch 是线程安全的，
 *          但业务 Context 若跨线程访问需自行同步。
 */
template <typename SM, typename State, typename Event>
class TimerDriven
{
public:
    using TimerWheelT = LSX_LIB::Timer::TimerWheel;

    TimerDriven(SM& fsm, TimerWheelT& timers)
        : fsm_(fsm), timers_(timers), alive_(std::make_shared<bool>(true))
    {
        fsm_.onTransition(
            [this](const State& from, const Event& e, const State& to, TransitionResult r)
            {
                (void)from; (void)e;
                if (r == TransitionResult::Ok) onEntered(to);
            });
        // 初始状态也启动超时
        onEntered(fsm_.currentState());
    }

    ~TimerDriven()
    {
        *alive_ = false;
        if (active_id_.isValid()) timers_.cancel(active_id_);
    }

    TimerDriven(const TimerDriven&) = delete;
    TimerDriven& operator=(const TimerDriven&) = delete;

    /// 注册：进入 s 后 ms 毫秒自动派发 e
    void autoTimeout(State s, std::int64_t ms, Event e)
    {
        defs_.push_back({s, ms, e});
        if (s == fsm_.currentState()) onEntered(s);
    }

private:
    struct Def { State s; std::int64_t ms; Event e; };

    void onEntered(State s)
    {
        if (active_id_.isValid()) { timers_.cancel(active_id_); active_id_ = {}; }
        for (const auto& d : defs_)
        {
            if (d.s == s)
            {
                auto alive = alive_;
                Event e = d.e;
                SM* fsm = &fsm_;
                active_id_ = timers_.scheduleOnce(d.ms, [alive, fsm, e]() {
                    if (*alive) fsm->dispatch(e);
                });
                return;
            }
        }
    }

    SM& fsm_;
    TimerWheelT& timers_;
    std::shared_ptr<bool> alive_;
    std::vector<Def> defs_;
    LSX_LIB::Timer::TimerId active_id_{};
};

} // namespace StateMachine
} // namespace LSX_LIB
