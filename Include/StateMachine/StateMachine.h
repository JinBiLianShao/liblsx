/**
 * @file StateMachine.h
 * @brief LIBLSX 工具库 - 通用可扩展有限状态机（FSM）
 * @details 提供 LSX_LIB::StateMachine 命名空间下的 StateMachine<State, Event, Context>：
 *
 *  - **完全泛型**：State / Event 可为枚举、字符串或任意可做 unordered_map 键的类型；
 *    Context 为业务上下文引用，守卫/动作直接访问。
 *  - **表驱动 + 运行时扩展**：addTransition 动态注册；支持精确 (from,event)、
 *    (ANY,event) 通配、以及 (from,event) 未命中时的父状态回退（层次状态机）。
 *  - **守卫与动作**：guard(Context&) -> bool；action(Context&) -> void；
 *    每个状态可设 onEntry / onExit；全局 onTransition 观察钩子。
 *  - **超时转换**：addTimeout(from, ms, event) 注册定时事件；由外部调用
 *    tick(now_ms) 驱动（无内部线程，嵌入式友好，也便于确定性测试）。
 *  - **可观测**：有限长度转换历史、toDot() 导出 Graphviz、统计与事件队列。
 *  - **重入安全**：动作内部可再次 dispatch（事件排队而非递归执行）；
 *    内部用 std::recursive_mutex 保护，避免回调重入死锁。
 *  - **错误风格**：dispatch 返回 TransitionResult，区分成功/无转换/守卫拒绝/严格模式错误。
 *
 * @author 连思鑫（jinbilianshao）
 * @date 2026-11-11
 * @version 1.0
 *
 * ### 最小示例
 * @code
 * enum class S { Idle, Running, Stopped };
 * enum class E { Start, Stop };
 * struct Ctx { int ticks = 0; };
 *
 * LSX_LIB::StateMachine::StateMachine<S, E, Ctx> fsm(S::Idle, ctx);
 * fsm.addTransition(S::Idle, E::Start, S::Running);
 * fsm.addTransition(S::Running, E::Stop, S::Stopped);
 * fsm.onEntry(S::Running, [](Ctx& c) { c.ticks = 0; });
 * fsm.dispatch(E::Start);   // -> Running
 * @endcode
 */
#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <sstream>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace LSX_LIB {
namespace StateMachine {

/// 转换结果
enum class TransitionResult : std::uint8_t
{
    Ok,              ///< 成功完成一次转换（含自转换）
    NoTransition,    ///< 当前状态 + 事件无匹配转换（宽容模式）
    GuardRejected,   ///< 匹配到转换但守卫返回 false
    StrictError,     ///< 严格模式下无匹配转换（视为错误）
    Busy             ///< 事件正在派发中，已排队（重入保护）
};

/// 通配状态标记：注册 (ANY, event) 转换，任何来源状态都可命中
template <typename State>
struct AnyState
{
    static constexpr bool is_any = true;
};

/// 状态节点（层次支持）
template <typename State, typename Event, typename Context>
struct StateNode
{
    using Action = std::function<void(Context&)>;

    State id{};
    State parent{};                 ///< 父状态（无父时等于 id 自身）
    bool has_parent = false;
    Action on_entry;
    Action on_exit;
};

/// 转换定义
template <typename State, typename Event, typename Context>
struct Transition
{
    using Guard = std::function<bool(Context&)>;
    using Action = std::function<void(Context&)>;

    State from{};
    Event event{};
    State to{};
    bool any_from = false;          ///< true = 通配来源
    Guard guard;                    ///< 可空
    Action action;                  ///< 可空（转移动作）
};

/// 转换历史记录
template <typename State, typename Event>
struct HistoryEntry
{
    State from{};
    Event event{};
    State to{};
    std::int64_t time_ms = 0;
    bool guard_rejected = false;
};

/**
 * @class StateMachine
 * @brief 通用有限状态机
 *
 * @tparam State 状态类型（需可哈希，或提供 std::hash 特化；枚举即可）
 * @tparam Event 事件类型（同上）
 * @tparam Context 业务上下文类型
 * @tparam Hash 哈希器（默认 std::hash<State>/std::hash<Event>）
 *
 * ### 线程安全 (@thread_safety)
 * 所有公开方法由内部 recursive_mutex 保护，多线程可安全调用；
 * dispatch 采用"事件队列 + 单线程泵"模型，动作回调在调用者线程串行执行，
 * 因此动作内部再次 dispatch 不会递归执行，而是排队后由当前泵继续处理。
 */
template <typename State, typename Event, typename Context,
          typename StateHash = std::hash<State>, typename EventHash = std::hash<Event>>
class StateMachine
{
public:
    using Guard = std::function<bool(Context&)>;
    using Action = std::function<void(Context&)>;
    using Observer = std::function<void(const State& from, const Event& e,
                                        const State& to, TransitionResult)>;
    using TransitionT = Transition<State, Event, Context>;
    using StateNodeT = StateNode<State, Event, Context>;
    using HistoryT = HistoryEntry<State, Event>;

    /// @param initial 初始状态
    /// @param ctx 业务上下文引用（生命周期须长于状态机）
    /// @param strict true = 无匹配转换返回 StrictError；false = 返回 NoTransition
    explicit StateMachine(State initial, Context& ctx, bool strict = false)
        : current_(initial), ctx_(ctx), strict_(strict), time_ms_(0) {}

    // ---- 配置（运行时可扩展） ----

    /// 注册父子关系：子状态未定义的转换会回退到父状态的转换
    void setParent(State child, State parent)
    {
        std::lock_guard<std::recursive_mutex> lk(mtx_);
        nodes_[child].id = child;
        nodes_[child].parent = parent;
        nodes_[child].has_parent = true;
    }

    /// 配置 onEntry / onExit
    void onEntry(State s, Action a)
    {
        std::lock_guard<std::recursive_mutex> lk(mtx_);
        nodes_[s].id = s;
        nodes_[s].on_entry = std::move(a);
    }
    void onExit(State s, Action a)
    {
        std::lock_guard<std::recursive_mutex> lk(mtx_);
        nodes_[s].id = s;
        nodes_[s].on_exit = std::move(a);
    }

    /// 注册转换（精确来源）
    void addTransition(State from, Event e, State to,
                       Guard guard = {}, Action action = {})
    {
        std::lock_guard<std::recursive_mutex> lk(mtx_);
        TransitionT t;
        t.from = from; t.event = e; t.to = to;
        t.any_from = false;
        t.guard = std::move(guard);
        t.action = std::move(action);
        transitions_.push_back(std::move(t));
    }

    /// 注册通配转换（任何来源状态收到 e 都转到 to）
    void addAnyTransition(Event e, State to,
                          Guard guard = {}, Action action = {})
    {
        std::lock_guard<std::recursive_mutex> lk(mtx_);
        TransitionT t;
        t.event = e; t.to = to;
        t.any_from = true;
        t.guard = std::move(guard);
        t.action = std::move(action);
        transitions_.push_back(std::move(t));
    }

    /// 注册超时：进入 from 后累计 ms 毫秒自动派发 event
    void addTimeout(State from, std::int64_t ms, Event e)
    {
        std::lock_guard<std::recursive_mutex> lk(mtx_);
        TimeoutDef td{from, ms, e};
        timeouts_.push_back(td);
        // 若当前就处于该状态，立即启动计时
        if (equal(from, current_)) armTimeoutsForCurrent();
    }

    /// 全局观察钩子（每次尝试转换后调用，含失败）
    void onTransition(Observer obs)
    {
        std::lock_guard<std::recursive_mutex> lk(mtx_);
        observers_.push_back(std::move(obs));
    }

    void setStrict(bool strict)
    {
        std::lock_guard<std::recursive_mutex> lk(mtx_);
        strict_ = strict;
    }

    // ---- 驱动 ----

    /// 派发事件。若在动作回调内部调用，会排队（返回 Busy）而非递归执行。
    TransitionResult dispatch(Event e)
    {
        std::lock_guard<std::recursive_mutex> lk(mtx_);
        queue_.push_back(e);
        if (pumping_)
            return TransitionResult::Busy;   // 事件已入队，由外层泵处理

        pumping_ = true;
        TransitionResult result = TransitionResult::NoTransition;
        while (!queue_.empty())
        {
            Event cur = queue_.front();
            queue_.pop_front();
            result = processOne(cur);
        }
        pumping_ = false;
        return result;
    }

    /// 时间驱动（嵌入式主循环里调用）：now_ms 单调递增毫秒
    /// 返回本次触发的超时事件数量
    std::size_t tick(std::int64_t now_ms)
    {
        std::lock_guard<std::recursive_mutex> lk(mtx_);
        time_ms_ = now_ms;
        if (active_timeout_.armed && now_ms >= active_timeout_.due_ms)
        {
            Event e = active_timeout_.event;
            active_timeout_.armed = false;
            dispatch(e);
            return 1;
        }
        return 0;
    }

    // ---- 查询 ----

    State currentState() const
    {
        std::lock_guard<std::recursive_mutex> lk(mtx_);
        return current_;
    }
    bool isIn(State s) const
    {
        std::lock_guard<std::recursive_mutex> lk(mtx_);
        return isInHierarchy(current_, s);
    }
    std::size_t transitionCount() const
    {
        std::lock_guard<std::recursive_mutex> lk(mtx_);
        return transitions_.size();
    }
    std::size_t historySize() const
    {
        std::lock_guard<std::recursive_mutex> lk(mtx_);
        return history_.size();
    }
    const std::deque<HistoryT>& history() const
    {
        std::lock_guard<std::recursive_mutex> lk(mtx_);
        return history_;
    }
    void clearHistory()
    {
        std::lock_guard<std::recursive_mutex> lk(mtx_);
        history_.clear();
    }
    void setHistoryLimit(std::size_t n)
    {
        std::lock_guard<std::recursive_mutex> lk(mtx_);
        history_limit_ = n;
        trimHistory();
    }

    /// 导出 Graphviz DOT（含状态、转换、父子关系）
    std::string toDot() const
    {
        std::lock_guard<std::recursive_mutex> lk(mtx_);
        std::ostringstream out;
        out << "digraph fsm {\n";
        out << "  rankdir=LR;\n";
        out << "  \"S*\" [label=\"ANY\", shape=box, style=dashed];\n";
        // 节点
        std::vector<State> seen;
        auto addNode = [&](const State& s) {
            for (const auto& x : seen) if (equal(x, s)) return;
            seen.push_back(s);
            out << "  \"S" << keyOf(s) << "\" [label=\"" << nameOf(s)
                << (equal(s, current_) ? "\" [shape=doublecircle]" : "\"") << "];\n";
        };
        addNode(current_);
        for (const auto& [k, node] : nodes_) { (void)k; addNode(node.id); }
        for (const auto& t : transitions_) { if (!t.any_from) addNode(t.from); addNode(t.to); }
        // 层次边
        for (const auto& [k, node] : nodes_)
        {
            (void)k;
            if (node.has_parent)
                out << "  \"S" << keyOf(node.id) << "\" -> \"S"
                    << keyOf(node.parent) << "\" [style=dotted];\n";
        }
        // 转换边（通配转换用 * 节点表示任意来源）
        for (const auto& t : transitions_)
        {
            out << "  \"S" << (t.any_from ? std::string("*") : "S" + keyOf(t.from))
                << "\" -> \"S" << keyOf(t.to)
                << "\" [label=\"" << nameOf(t.event)
                << (t.guard ? " [g]" : "") << "\"];\n";
        }
        out << "}\n";
        return out.str();
    }

    /// 手动强制设置状态（不触发动作/历史，仅用于恢复/测试）
    void forceState(State s)
    {
        std::lock_guard<std::recursive_mutex> lk(mtx_);
        current_ = s;
        armTimeoutsForCurrent();
    }

private:
    struct TimeoutDef
    {
        State from{};
        std::int64_t ms = 0;
        Event event{};
    };
    struct ActiveTimeout
    {
        bool armed = false;
        std::int64_t due_ms = 0;
        Event event{};
    };

    bool equal(const State& a, const State& b) const { return a == b; }

    bool isInHierarchy(State child, State ancestor) const
    {
        if (equal(child, ancestor)) return true;
        auto it = nodes_.find(child);
        std::size_t guard = 0;
        while (it != nodes_.end() && it->second.has_parent && guard++ < 32)
        {
            State p = it->second.parent;
            if (equal(p, ancestor)) return true;
            it = nodes_.find(p);
        }
        return false;
    }

    /// 找到匹配的转换：精确优先 → 父状态回退 → 通配
    const TransitionT* findTransition(State from, Event e) const
    {
        // 1) 精确匹配（后注册优先，便于覆盖）
        for (auto it = transitions_.rbegin(); it != transitions_.rend(); ++it)
        {
            if (!it->any_from && equal(it->from, from) && it->event == e)
                return &(*it);
        }
        // 2) 父状态回退（层次状态机）
        auto nit = nodes_.find(from);
        std::size_t guard = 0;
        while (nit != nodes_.end() && nit->second.has_parent && guard++ < 32)
        {
            State p = nit->second.parent;
            for (auto it = transitions_.rbegin(); it != transitions_.rend(); ++it)
            {
                if (!it->any_from && equal(it->from, p) && it->event == e)
                    return &(*it);
            }
            nit = nodes_.find(p);
        }
        // 3) 通配
        for (auto it = transitions_.rbegin(); it != transitions_.rend(); ++it)
        {
            if (it->any_from && it->event == e) return &(*it);
        }
        return nullptr;
    }

    TransitionResult processOne(Event e)
    {
        State from = current_;
        const TransitionT* t = findTransition(from, e);
        if (!t)
        {
            notify(from, e, from, strict_ ? TransitionResult::StrictError
                                          : TransitionResult::NoTransition);
            return strict_ ? TransitionResult::StrictError
                           : TransitionResult::NoTransition;
        }
        if (t->guard && !t->guard(ctx_))
        {
            HistoryT h{from, e, from, time_ms_, true};
            pushHistory(h);
            notify(from, e, from, TransitionResult::GuardRejected);
            return TransitionResult::GuardRejected;
        }

        State to = t->to;
        // onExit：按 UML 语义，自转换（from==to）同样触发 exit + entry
        {
            auto it = nodes_.find(from);
            if (it != nodes_.end() && it->second.on_exit) it->second.on_exit(ctx_);
        }
        // 转移动作
        if (t->action) t->action(ctx_);

        current_ = to;
        // onEntry
        {
            auto it = nodes_.find(to);
            if (it != nodes_.end() && it->second.on_entry) it->second.on_entry(ctx_);
        }

        armTimeoutsForCurrent();
        HistoryT h{from, e, to, time_ms_, false};
        pushHistory(h);
        notify(from, e, to, TransitionResult::Ok);
        return TransitionResult::Ok;
    }

    void armTimeoutsForCurrent()
    {
        active_timeout_.armed = false;
        for (const auto& td : timeouts_)
        {
            if (equal(td.from, current_))
            {
                active_timeout_.armed = true;
                active_timeout_.due_ms = time_ms_ + td.ms;
                active_timeout_.event = td.event;
                return;
            }
        }
    }

    void pushHistory(const HistoryT& h)
    {
        history_.push_back(h);
        trimHistory();
    }
    void trimHistory()
    {
        while (history_.size() > history_limit_) history_.pop_front();
    }
    void notify(const State& from, const Event& e, const State& to, TransitionResult r)
    {
        for (auto& obs : observers_) if (obs) obs(from, e, to, r);
    }

    /// 键/名字符串化：枚举走底层整数，字符串直接用
    template <typename T>
    static std::string keyOf(const T& v)
    {
        if constexpr (std::is_enum_v<T>)
            return std::to_string(static_cast<std::int64_t>(v));
        else if constexpr (std::is_convertible_v<T, std::string>)
            return std::string(v);
        else
            return std::to_string(static_cast<std::int64_t>(v));
    }
    template <typename T>
    static std::string nameOf(const T& v) { return keyOf(v); }

    mutable std::recursive_mutex mtx_;
    State current_;
    Context& ctx_;
    bool strict_;
    std::int64_t time_ms_;

    std::unordered_map<State, StateNodeT, StateHash> nodes_;
    std::vector<TransitionT> transitions_;
    std::vector<TimeoutDef> timeouts_;
    ActiveTimeout active_timeout_;
    std::vector<Observer> observers_;
    std::deque<HistoryT> history_;
    std::size_t history_limit_ = 64;
    std::deque<Event> queue_;
    bool pumping_ = false;
};

} // namespace StateMachine
} // namespace LSX_LIB
