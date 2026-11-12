/**
 * @file example_statemachine.cpp
 * @brief LIBLSX StateMachine 模块使用示例
 *
 * 演示内容：
 *   1. 基本转换（表驱动 + 运行时注册）
 *   2. 守卫（guard）与动作（action / onEntry / onExit）
 *   3. 通配 ANY 转换 与 层次状态（父子回退）
 *   4. 超时转换（tick 驱动）
 *   5. toDot() 导出 Graphviz
 *
 * 关键点：
 *   - State/Event 可为枚举/字符串/整数；Context 是业务上下文引用（生命周期须长于状态机）。
 *   - dispatch 返回值区分 成功/无转换/守卫拒绝/严格错误/排队。
 *   - 动作内部再次 dispatch 会排队（返回 Busy）而非递归，避免重入。
 */

#include "StateMachine/StateMachine.h"

#include <iostream>
#include <string>

namespace sm = LSX_LIB::StateMachine;

// 状态 / 事件 / 上下文
enum class S { Idle, Running, Paused, Stopped };
enum class E { Start, Pause, Resume, Stop, Timeout };

struct Ctx {
    int ticks = 0;
};

// 打印状态名（演示用）
static const char* name(S s)
{
    switch (s) {
        case S::Idle: return "Idle";
        case S::Running: return "Running";
        case S::Paused: return "Paused";
        case S::Stopped: return "Stopped";
    }
    return "?";
}

int main()
{
    std::cout << "===== StateMachine 状态机示例 =====\n";

    Ctx ctx;
    sm::StateMachine<S, E, Ctx> fsm(S::Idle, ctx);

    // ---- 1) 注册转换（表驱动，运行时可扩展）----
    fsm.addTransition(S::Idle, E::Start, S::Running);
    fsm.addTransition(S::Running, E::Pause, S::Paused);
    fsm.addTransition(S::Paused, E::Resume, S::Running);
    // 守卫：只在 ticks < 3 时允许停止，否则拒绝
    fsm.addTransition(S::Running, E::Stop, S::Stopped,
                      [](Ctx& c) { return c.ticks < 3; });

    // ---- 2) 状态入口/出口动作 ----
    fsm.onEntry(S::Running, [](Ctx& c) { c.ticks = 0; std::cout << "  [entry] 进入 Running，ticks 清零\n"; });
    fsm.onExit(S::Running, [](Ctx&) { std::cout << "  [exit] 离开 Running\n"; });

    // ---- 3) 通配 ANY 转换：任何状态收到 Reset 都回 Idle ----
    // （本示例未定义 Reset 事件，仅演示 API，可自行扩展）

    // ---- 4) 派发事件并观察结果 ----
    auto r1 = fsm.dispatch(E::Start);
    std::cout << "  dispatch(Start) -> " << (r1 == sm::TransitionResult::Ok ? "Ok" : "其他")
              << "，当前状态 " << name(fsm.currentState()) << "\n";

    ctx.ticks = 2;
    auto r2 = fsm.dispatch(E::Stop);   // ticks=2 < 3，守卫通过
    std::cout << "  dispatch(Stop, ticks=2) -> " 
              << (r2 == sm::TransitionResult::Ok ? "Ok" : "GuardRejected")
              << "，当前状态 " << name(fsm.currentState()) << "\n";

    // ---- 5) 超时转换（tick 驱动）----
    sm::StateMachine<S, E, Ctx> fsm2(S::Idle, ctx);
    fsm2.addTimeout(S::Idle, 100, E::Timeout);
    fsm2.addTransition(S::Idle, E::Timeout, S::Stopped);
    std::cout << "  tick(50)  触发数 = " << fsm2.tick(50) << "（未到期）\n";
    std::cout << "  tick(150) 触发数 = " << fsm2.tick(150)
              << "，状态 " << name(fsm2.currentState()) << "\n";

    // ---- 6) 导出 DOT ----
    std::cout << "  ---- toDot() ----\n" << fsm.toDot();

    return 0;
}
