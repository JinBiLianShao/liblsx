/**
 * @file test_statemachine.cpp
 * @brief StateMachine 深度测试：通用性/可扩展/守卫/动作/层次/通配/超时/历史/重入/并发/DOT
 */
#include "test_framework.h"

#include <atomic>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

#include "StateMachine/StateMachine.h"
#include "StateMachine/StateMachineTimer.h"
#include "Timer/Timer.h"

using namespace LSX_LIB::StateMachine;

namespace
{
enum class S { Idle, Running, Paused, Stopped, Fault };
enum class E { Start, Pause, Resume, Stop, Reset, Error, Timeout, Tick };
struct Ctx
{
    int entries = 0;
    int exits = 0;
    int transitions = 0;
    int ticks = 0;
    bool allowed = true;
    std::vector<std::string> log;
};
}

LSX_TEST(StateMachine, BasicTransitions) {
    Ctx c;
    StateMachine<S, E, Ctx> fsm(S::Idle, c);
    fsm.addTransition(S::Idle, E::Start, S::Running);
    fsm.addTransition(S::Running, E::Stop, S::Stopped);
    LSX_EXPECT_EQ(fsm.dispatch(E::Start), TransitionResult::Ok);
    LSX_EXPECT_EQ(fsm.currentState(), S::Running);
    LSX_EXPECT_EQ(fsm.dispatch(E::Stop), TransitionResult::Ok);
    LSX_EXPECT_EQ(fsm.currentState(), S::Stopped);
}

LSX_TEST(StateMachine, NoTransitionAndStrictMode) {
    Ctx c;
    StateMachine<S, E, Ctx> fsm(S::Idle, c);
    LSX_EXPECT_EQ(fsm.dispatch(E::Stop), TransitionResult::NoTransition);
    LSX_EXPECT_EQ(fsm.currentState(), S::Idle);
    fsm.setStrict(true);
    LSX_EXPECT_EQ(fsm.dispatch(E::Stop), TransitionResult::StrictError);
}

LSX_TEST(StateMachine, EntryAndExitActions) {
    Ctx c;
    StateMachine<S, E, Ctx> fsm(S::Idle, c);
    fsm.onEntry(S::Running, [](Ctx& x) { ++x.entries; x.log.push_back("enter Running"); });
    fsm.onExit(S::Running, [](Ctx& x) { ++x.exits; x.log.push_back("exit Running"); });
    fsm.addTransition(S::Idle, E::Start, S::Running);
    fsm.addTransition(S::Running, E::Stop, S::Stopped);
    fsm.dispatch(E::Start);
    LSX_EXPECT_EQ(c.entries, 1);
    fsm.dispatch(E::Stop);
    LSX_EXPECT_EQ(c.exits, 1);
    LSX_EXPECT_EQ(c.log.size(), 2u);
}

LSX_TEST(StateMachine, TransitionActionAndGuard) {
    Ctx c;
    StateMachine<S, E, Ctx> fsm(S::Idle, c);
    fsm.addTransition(S::Idle, E::Start, S::Running,
        [](Ctx& x) { return x.allowed; },              // guard
        [](Ctx& x) { ++x.transitions; x.log.push_back("go"); });  // action
    c.allowed = false;
    LSX_EXPECT_EQ(fsm.dispatch(E::Start), TransitionResult::GuardRejected);
    LSX_EXPECT_EQ(fsm.currentState(), S::Idle);
    LSX_EXPECT_EQ(c.transitions, 0);
    c.allowed = true;
    LSX_EXPECT_EQ(fsm.dispatch(E::Start), TransitionResult::Ok);
    LSX_EXPECT_EQ(c.transitions, 1);
}

LSX_TEST(StateMachine, GuardRejectedRecordedInHistory) {
    Ctx c;
    StateMachine<S, E, Ctx> fsm(S::Idle, c);
    fsm.addTransition(S::Idle, E::Start, S::Running, [](Ctx&) { return false; });
    fsm.dispatch(E::Start);
    LSX_EXPECT_EQ(fsm.historySize(), 1u);
    LSX_EXPECT_TRUE(fsm.history().back().guard_rejected);
}

LSX_TEST(StateMachine, SelfTransitionTriggersExitEntry) {
    Ctx c;
    StateMachine<S, E, Ctx> fsm(S::Running, c);
    fsm.onExit(S::Running, [](Ctx& x) { ++x.exits; });
    fsm.onEntry(S::Running, [](Ctx& x) { ++x.entries; });
    fsm.addTransition(S::Running, E::Tick, S::Running, {}, [](Ctx& x) { ++x.ticks; });
    fsm.dispatch(E::Tick);
    LSX_EXPECT_EQ(c.exits, 1);
    LSX_EXPECT_EQ(c.entries, 1);
    LSX_EXPECT_EQ(c.ticks, 1);
    LSX_EXPECT_EQ(fsm.currentState(), S::Running);
}

LSX_TEST(StateMachine, AnyTransitionWildcard) {
    Ctx c;
    StateMachine<S, E, Ctx> fsm(S::Running, c);
    fsm.addAnyTransition(E::Error, S::Fault);
    LSX_EXPECT_EQ(fsm.dispatch(E::Error), TransitionResult::Ok);
    LSX_EXPECT_EQ(fsm.currentState(), S::Fault);
    fsm.addTransition(S::Fault, E::Reset, S::Idle);
    fsm.dispatch(E::Reset);
    LSX_EXPECT_EQ(fsm.currentState(), S::Idle);
    LSX_EXPECT_EQ(fsm.dispatch(E::Error), TransitionResult::Ok);  // 任意状态都能 Fault
}

LSX_TEST(StateMachine, ExactBeatsWildcard) {
    Ctx c;
    StateMachine<S, E, Ctx> fsm(S::Idle, c);
    fsm.addAnyTransition(E::Error, S::Fault);
    fsm.addTransition(S::Idle, E::Error, S::Stopped);   // 精确优先
    fsm.dispatch(E::Error);
    LSX_EXPECT_EQ(fsm.currentState(), S::Stopped);
}

LSX_TEST(StateMachine, HierarchicalParentFallback) {
    Ctx c;
    StateMachine<S, E, Ctx> fsm(S::Paused, c);
    fsm.setParent(S::Paused, S::Running);      // Paused 是 Running 的子状态
    fsm.addTransition(S::Running, E::Stop, S::Stopped);  // 父定义，子继承
    LSX_EXPECT_TRUE(fsm.isIn(S::Running));
    LSX_EXPECT_EQ(fsm.dispatch(E::Stop), TransitionResult::Ok);
    LSX_EXPECT_EQ(fsm.currentState(), S::Stopped);
}

LSX_TEST(StateMachine, ChildOverridesParent) {
    Ctx c;
    StateMachine<S, E, Ctx> fsm(S::Paused, c);
    fsm.setParent(S::Paused, S::Running);
    fsm.addTransition(S::Running, E::Reset, S::Idle);
    fsm.addTransition(S::Paused, E::Reset, S::Fault);   // 子覆盖
    fsm.dispatch(E::Reset);
    LSX_EXPECT_EQ(fsm.currentState(), S::Fault);
}

LSX_TEST(StateMachine, TimeoutViaTick) {
    Ctx c;
    StateMachine<S, E, Ctx> fsm(S::Idle, c);
    fsm.addTransition(S::Idle, E::Timeout, S::Fault);
    fsm.addTimeout(S::Idle, 100, E::Timeout);
    LSX_EXPECT_EQ(fsm.tick(50), 0u);
    LSX_EXPECT_EQ(fsm.currentState(), S::Idle);
    LSX_EXPECT_EQ(fsm.tick(150), 1u);
    LSX_EXPECT_EQ(fsm.currentState(), S::Fault);
}

LSX_TEST(StateMachine, TimeoutRearmedOnReentry) {
    Ctx c;
    StateMachine<S, E, Ctx> fsm(S::Idle, c);
    fsm.addTransition(S::Idle, E::Tick, S::Idle);
    fsm.addTransition(S::Idle, E::Timeout, S::Fault);
    fsm.addTimeout(S::Idle, 100, E::Timeout);
    fsm.tick(80);
    fsm.dispatch(E::Tick);           // 自转换重排超时：从当前时刻重新计时
    LSX_EXPECT_EQ(fsm.tick(170), 0u);
    LSX_EXPECT_EQ(fsm.tick(181), 1u);
    LSX_EXPECT_EQ(fsm.currentState(), S::Fault);
}

LSX_TEST(StateMachine, ReentrantDispatchIsQueued) {
    Ctx c;
    StateMachine<S, E, Ctx> fsm(S::Idle, c);
    std::vector<std::string> order;
    fsm.addTransition(S::Idle, E::Start, S::Running, {}, [&](Ctx&) {
        order.push_back("start-action");
        auto r = fsm.dispatch(E::Stop);           // 动作内重入派发
        LSX_EXPECT_EQ(r, TransitionResult::Busy); // 排队而非递归
        order.push_back("after-dispatch");
    });
    fsm.addTransition(S::Running, E::Stop, S::Stopped, {}, [&](Ctx&) {
        order.push_back("stop-action");
    });
    fsm.dispatch(E::Start);
    LSX_EXPECT_EQ(fsm.currentState(), S::Stopped);
    LSX_EXPECT_EQ(order.size(), 3u);
    LSX_EXPECT_EQ(order[0], std::string("start-action"));
    LSX_EXPECT_EQ(order[2], std::string("stop-action"));
}

LSX_TEST(StateMachine, ObserverSeesAllAttempts) {
    Ctx c;
    StateMachine<S, E, Ctx> fsm(S::Idle, c);
    int ok = 0, rejected = 0, noTrans = 0;
    fsm.onTransition([&](const S&, const E&, const S&, TransitionResult r) {
        if (r == TransitionResult::Ok) ++ok;
        else if (r == TransitionResult::GuardRejected) ++rejected;
        else ++noTrans;
    });
    fsm.addTransition(S::Idle, E::Start, S::Running, [](Ctx&) { return false; });
    fsm.dispatch(E::Start);   // guard 拒绝
    fsm.dispatch(E::Stop);    // 无转换
    fsm.addTransition(S::Idle, E::Reset, S::Idle);
    fsm.dispatch(E::Reset);   // 成功
    LSX_EXPECT_EQ(ok, 1);
    LSX_EXPECT_EQ(rejected, 1);
    LSX_EXPECT_EQ(noTrans, 1);
}

LSX_TEST(StateMachine, HistoryAndLimit) {
    Ctx c;
    StateMachine<S, E, Ctx> fsm(S::Idle, c);
    fsm.setHistoryLimit(3);
    fsm.addTransition(S::Idle, E::Start, S::Running);
    fsm.addTransition(S::Running, E::Pause, S::Paused);
    fsm.addTransition(S::Paused, E::Resume, S::Running);
    fsm.addTransition(S::Running, E::Stop, S::Stopped);
    fsm.dispatch(E::Start); fsm.dispatch(E::Pause);
    fsm.dispatch(E::Resume); fsm.dispatch(E::Stop);
    LSX_EXPECT_EQ(fsm.historySize(), 3u);
    LSX_EXPECT_EQ(fsm.history().front().event, E::Pause);
    fsm.clearHistory();
    LSX_EXPECT_EQ(fsm.historySize(), 0u);
}

LSX_TEST(StateMachine, DotExportContainsStatesAndEdges) {
    Ctx c;
    StateMachine<S, E, Ctx> fsm(S::Idle, c);
    fsm.addTransition(S::Idle, E::Start, S::Running);
    fsm.setParent(S::Paused, S::Running);
    auto dot = fsm.toDot();
    LSX_EXPECT_TRUE(dot.find("digraph fsm") != std::string::npos);
    LSX_EXPECT_TRUE(dot.find("->") != std::string::npos);
    LSX_EXPECT_TRUE(dot.find("doublecircle") != std::string::npos);  // 当前状态标记
    LSX_EXPECT_TRUE(dot.find("dotted") != std::string::npos);        // 层次边
}

LSX_TEST(StateMachine, GenericStringStateAndEvent) {
    struct SCtx { int n = 0; };
    SCtx c;
    StateMachine<std::string, std::string, SCtx> fsm("idle", c);
    fsm.addTransition("idle", "start", "running", {}, [](SCtx& x) { ++x.n; });
    fsm.addAnyTransition("fatal", "fault");
    LSX_EXPECT_EQ(fsm.dispatch("start"), TransitionResult::Ok);
    LSX_EXPECT_EQ(fsm.currentState(), std::string("running"));
    LSX_EXPECT_EQ(c.n, 1);
    fsm.dispatch("fatal");
    LSX_EXPECT_EQ(fsm.currentState(), std::string("fault"));
}

LSX_TEST(StateMachine, GenericIntegerStateAndEvent) {
    struct ICtx { int hits = 0; };
    ICtx c;
    StateMachine<int, int, ICtx> fsm(0, c);
    fsm.addTransition(0, 1, 10);
    fsm.addTransition(10, 2, 20, {}, [](ICtx& x) { ++x.hits; });
    fsm.dispatch(1);
    LSX_EXPECT_EQ(fsm.currentState(), 10);
    fsm.dispatch(2);
    LSX_EXPECT_EQ(fsm.currentState(), 20);
    LSX_EXPECT_EQ(c.hits, 1);
}

LSX_TEST(StateMachine, ForceStateForRecovery) {
    Ctx c;
    StateMachine<S, E, Ctx> fsm(S::Idle, c);
    fsm.onEntry(S::Fault, [](Ctx& x) { ++x.entries; });
    fsm.forceState(S::Fault);
    LSX_EXPECT_EQ(fsm.currentState(), S::Fault);
    LSX_EXPECT_EQ(c.entries, 0);      // forceState 不触发动作
    LSX_EXPECT_EQ(fsm.historySize(), 0u);
}

LSX_TEST(StateMachine, ConcurrentDispatchSerialized) {
    Ctx c;
    StateMachine<S, E, Ctx> fsm(S::Idle, c);
    std::atomic<int> ok{0};
    fsm.addTransition(S::Idle, E::Tick, S::Idle, {}, [&](Ctx& x) { ++x.ticks; ++ok; });
    std::vector<std::thread> ts;
    for (int t = 0; t < 4; ++t)
        ts.emplace_back([&]() { for (int i = 0; i < 100; ++i) fsm.dispatch(E::Tick); });
    for (auto& t : ts) t.join();
    LSX_EXPECT_EQ(ok.load(), 400);
    LSX_EXPECT_EQ(c.ticks, 400);
    LSX_EXPECT_EQ(fsm.currentState(), S::Idle);
}

LSX_TEST(StateMachineTimer, TimerWheelDrivesTimeout) {
    struct C { std::atomic<int> fired{0}; };
    C c;
    LSX_LIB::Timer::TimerWheel timers;
    timers.start();
    StateMachine<S, E, C> fsm(S::Idle, c);
    fsm.addTransition(S::Idle, E::Timeout, S::Fault, {}, [](C& x) { ++x.fired; });
    {
        TimerDriven<decltype(fsm), S, E> driven(fsm, timers);
        driven.autoTimeout(S::Idle, 40, E::Timeout);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    timers.shutdown();
    LSX_EXPECT_EQ(fsm.currentState(), S::Fault);
    LSX_EXPECT_EQ(c.fired.load(), 1);
}
