# LIBLSX 工具库 StateMachine 模块使用说明（通用可扩展状态机）

## 模块概述

StateMachine<State, Event, Context> 是一个 **header-only、零第三方依赖** 的通用有限状态机，面向嵌入式 Linux 的设备控制、协议会话、流程编排。命名空间：LSX_LIB::StateMachine。

| 目标 | 实现方式 |
|------|----------|
| 泛型 | State/Event 可为枚举、字符串、整数或任意可哈希类型；Context 为业务数据引用 |
| 可扩展 | 转换表运行时注册（addTransition / setParent / addTimeout），无需改类 |
| 层次状态 | 子状态未定义转换时回退到父状态（父子继承 + 子覆盖） |
| 可观测 | 转换历史、全局观察钩子、Graphviz DOT 导出 |
| 可组合 | 可选 TimerWheel 适配器实现真实时钟驱动超时 |
| 嵌入式友好 | 无内部线程、tick() 由主循环驱动，确定性、无动态分配热路径 |
| 重入安全 | 动作内再次 dispatch 排队而非递归；recursive_mutex 保护 |

## 快速上手

    #include "StateMachine/StateMachine.h"

    enum class S { Idle, Running, Stopped };
    enum class E { Start, Stop };
    struct Ctx { int ticks = 0; };

    Ctx ctx;
    LSX_LIB::StateMachine::StateMachine<S, E, Ctx> fsm(S::Idle, ctx);

    fsm.addTransition(S::Idle, E::Start, S::Running);
    fsm.addTransition(S::Running, E::Stop, S::Stopped);
    fsm.onEntry(S::Running, [](Ctx& c) { c.ticks = 0; });

    fsm.dispatch(E::Start);        // -> Running（触发 onEntry）

## 转换配置

### 精确转换与守卫/动作

    fsm.addTransition(S::Idle, E::Start, S::Running,
        [](Ctx& c) { return c.enabled; },                 // guard，可空
        [](Ctx& c) { c.log.push_back("started"); });     // transition action，可空

守卫返回 false 时状态不变（GuardRejected），历史中记录一条拒绝记录。动作按 onExit(from) → transition action → onEntry(to) 顺序执行；**自转换（from==to）同样触发 exit+entry**（UML 语义）。

### 通配转换（任意来源）

    fsm.addAnyTransition(E::Error, S::Fault);   // 任何状态收到 Error 都进 Fault

匹配优先级：**精确 (from,event) > 父状态回退 > 通配 ANY**。同键后注册优先，便于覆盖。

### 层次状态（父子继承）

    fsm.setParent(S::Paused, S::Running);
    fsm.addTransition(S::Running, E::Stop, S::Stopped);  // 父定义
    // 处于 Paused 时收到 Stop 会命中父转换（子未定义时回退）
    fsm.addTransition(S::Paused, E::Reset, S::Fault);    // 子可覆盖特定事件
    fsm.isIn(S::Running);   // 层次查询：Paused 时也返回 true

## 超时转换

两种驱动方式，按工程选择：

**① 主循环 tick（确定性、无线程，嵌入式首选）**

    fsm.addTransition(S::Idle, E::Timeout, S::Fault);
    fsm.addTimeout(S::Idle, 100, E::Timeout);
    fsm.tick(now_ms);        // 主循环用单调毫秒驱动；到期自动 dispatch，返回触发数

进入状态即启动计时；自转换会**重新计时**（防抖语义）。

**② TimerWheel 适配器（真实时钟自动驱动）**

    #include "StateMachine/StateMachineTimer.h"
    LSX_LIB::Timer::TimerWheel timers; timers.start();
    LSX_LIB::StateMachine::TimerDriven<decltype(fsm), S, E> driven(fsm, timers);
    driven.autoTimeout(S::Idle, 1000, E::Timeout);

定时器回调运行在 TimerWheel 工作线程；dispatch 本身线程安全，但业务 Context 若跨线程访问需自行同步。

## 可观测性

    fsm.onTransition([](const S& from, const E& e, const S& to, TransitionResult r) {
        // 每次尝试（含失败/守卫拒绝）都会回调
    });

    fsm.setHistoryLimit(64);         // 环形历史，默认 64
    const auto& h = fsm.history();  // 含 from/event/to/time_ms/guard_rejected
    fsm.toDot();                     // 导出 Graphviz DOT（含层次边、当前状态标记）

## 重入与并发

- 动作回调里再次 dispatch(e)：事件入队，返回 Busy，由当前派发泵继续处理，**不递归**。
- 所有公开方法由 recursive_mutex 保护；多线程 dispatch 串行执行（并发测试 4×100 事件全过）。

## 错误处理

dispatch() 返回 TransitionResult：

| 值 | 含义 |
|----|------|
| Ok | 完成一次转换（含自转换） |
| NoTransition | 无匹配转换（宽容模式，默认） |
| StrictError | 严格模式下无匹配（setStrict(true)） |
| GuardRejected | 守卫拒绝 |
| Busy | 动作内重入派发，已排队 |

## 状态类型支持

- 枚举类（推荐）：enum class S { ... }
- 字符串：StateMachine<std::string, std::string, Ctx>
- 整数：StateMachine<int, int, Ctx>
- 自定义类型：需可 == 比较 + 提供 std::hash 特化

## 常见错误用法与规避

1. **Context 生命周期**：状态机持有 Context&，上下文必须活得比状态机久。
2. **在动作里做重活**：动作在 dispatch 调用者线程串行执行，重逻辑请投递线程池。
3. **通配转换遮住精确逻辑**：注册顺序上精确优先，但通配仍会捕获所有未定义事件——先用精确覆盖关键路径。
4. **超时与状态进入不同步**：addTimeout 只对注册后进入的状态生效；forceState() 恢复状态时会重新 arm。
5. **TimerDriven 与 tick 混用**：二选一，避免同一状态被两条超时路径重复触发。
