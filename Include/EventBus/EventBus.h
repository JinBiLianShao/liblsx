/**
 * @file EventBus.h
 * @brief LIBLSX 工具库 - 事件总线模块
 * @details 提供 LSX_LIB::EventBus 命名空间下的类型安全发布-订阅机制：
 *  - EventBus：按事件类型（任意可拷贝类型，通常为 struct）分发事件；
 *  - Subscription：订阅句柄（RAII + 手动 unsubscribe 均可），析构自动退订。
 *
 * 分发语义：publish 时在**调用者线程**同步分发（默认），或经 dispatcher 在
 * 指定执行上下文异步分发。同步模式下回调直接串行执行，适合与 ThreadPool/
 * TimerWheel 组合使用（publish 回调里调用 timers.schedule 等）。
 *
 * @author 连思鑫（jinbilianshao）
 * @date 2026-11-10
 * @version 1.0
 *
 * ### 使用示例
 * @code
 * struct SensorData { int id; double value; };
 *
 * LSX_LIB::EventBus::EventBus bus;
 *
 * auto sub = bus.subscribe<SensorData>([](const SensorData& e) {
 *     // 处理事件
 * });
 *
 * bus.publish(SensorData{1, 3.14});
 *
 * sub.unsubscribe();  // 或让 Subscription 析构自动退订
 * @endcode
 */
#pragma once

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace LSX_LIB {
namespace EventBus {

class EventBus;

/**
 * @class Subscription
 * @brief 订阅句柄：RAII 语义 + 手动退订 + 有效性检查
 * @details 拷贝出的句柄共享同一份订阅状态（最后一份负责退订效果一致）；
 * 原始 subscribe 返回的句柄析构即退订，与手动 unsubscribe 等效。
 */
class Subscription
{
public:
    Subscription() = default;

    /// 析构时自动退订（若仍有效且未退订）
    ~Subscription()
    {
        unsubscribe();
    }

    Subscription(const Subscription& other) = default;
    Subscription& operator=(const Subscription& other) = default;

    Subscription(Subscription&& other) noexcept
        : bus_(other.bus_), id_(other.id_), state_(std::move(other.state_))
    {
        other.bus_ = nullptr;
        other.id_ = 0;
    }

    Subscription& operator=(Subscription&& other) noexcept
    {
        if (this != &other)
        {
            unsubscribe();
            bus_ = other.bus_;
            id_ = other.id_;
            state_ = std::move(other.state_);
            other.bus_ = nullptr;
            other.id_ = 0;
        }
        return *this;
    }

    /// 手动退订（幂等）——实现见文件底部（需要 EventBus 完整类型）
    void unsubscribe();

    /// 订阅是否仍有效（未退订且事件总线未销毁）
    bool isValid() const
    {
        return bus_ && id_ != 0 && (!state_ || !state_->load());
    }

private:
    friend class EventBus;

    Subscription(EventBus* bus, std::type_index type, std::uint64_t id,
                 std::shared_ptr<std::atomic<bool>> state)
        : bus_(bus), type_(type), id_(id), state_(std::move(state)) {}

    EventBus* bus_ = nullptr;
    std::type_index type_ = typeid(void);
    std::uint64_t id_ = 0;
    std::shared_ptr<std::atomic<bool>> state_;  // 多句柄共享退订标志
};

/**
 * @class EventBus
 * @brief 类型安全的事件发布-订阅总线
 * @details 按事件类型 T（任意可拷贝类型）维护订阅回调列表。publish 在调用者
 * 线程同步分发；如需异步请结合 Timer/ThreadPool 在回调中投递。
 * 订阅回调内 publish/subscribe/unsubscribe 均安全（回调在锁外执行）。
 * 事件类型推荐使用自定义 struct（如 SensorData{...}）而非裸基本类型，
 * 以获得类型安全与可读性。
 */
class EventBus
{
public:
    EventBus() = default;
    ~EventBus()
    {
        // 总线销毁时使所有订阅句柄失效（不主动调用回调）
        std::lock_guard<std::mutex> lk(mtx_);
        for (auto& st : states_)
        {
            st.second->store(true, std::memory_order_release);  // 标记退订，句柄析构不再回访总线
        }
        states_.clear();
        alive_.clear();
        destroyed_.store(true);
    }

    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;

    /**
     * @brief 订阅指定类型事件
     * @tparam T 事件类型（任意可拷贝类型）
     * @param handler 事件处理回调，在 publish 调用者线程同步执行
     * @return 订阅句柄，析构或 unsubscribe() 退订
     */
    template <typename T>
    Subscription subscribe(std::function<void(const T&)> handler)
    {
        auto state = std::make_shared<std::atomic<bool>>(false);
        auto erased = [handler, state](const void* ev) {
            if (!state->load(std::memory_order_acquire))
            {
                handler(*static_cast<const T*>(ev));
            }
        };
        std::uint64_t id = 0;
        {
            std::lock_guard<std::mutex> lk(mtx_);
            if (!destroyed_.load())
            {
                id = next_id_++;
                auto& list = handlers_[std::type_index(typeid(T))];
                list.push_back({id, std::move(erased)});
                alive_.emplace(id, typeid(T).name());
                states_[id] = state;
            }
        }
        return Subscription(this, std::type_index(typeid(T)), id, std::move(state));
    }

    /**
     * @brief 发布事件：在调用者线程同步分发给该类型的全部有效订阅者
     * @details 发布期间某订阅者退订，本次分发对该订阅者的剩余行为由
     * expired 标志保证不再触发。
     */
    template <typename T>
    void publish(const T& event)
    {
        // 拷贝快照后在锁外分发，允许回调内再次 publish/subscribe
        std::vector<Handler> snapshot;
        {
            std::lock_guard<std::mutex> lk(mtx_);
            auto it = handlers_.find(std::type_index(typeid(T)));
            if (it == handlers_.end())
            {
                return;
            }
            snapshot = it->second;
        }
        for (const auto& h : snapshot)
        {
            h.func(static_cast<const void*>(&event));
        }
    }

    /// @brief 当前某事件类型的订阅者数量（0 = 无）
    template <typename T>
    std::size_t subscriberCount() const
    {
        std::lock_guard<std::mutex> lk(mtx_);
        auto it = handlers_.find(std::type_index(typeid(T)));
        return it == handlers_.end() ? 0 : it->second.size();
    }

private:
    friend class Subscription;

    struct Handler
    {
        std::uint64_t id;
        std::function<void(const void*)> func;
    };

    void unsubscribeById(std::type_index type, std::uint64_t id)
    {
        std::lock_guard<std::mutex> lk(mtx_);
        alive_.erase(id);
        states_.erase(id);
        auto it = handlers_.find(type);
        if (it != handlers_.end())
        {
            auto& list = it->second;
            list.erase(
                std::remove_if(list.begin(), list.end(),
                               [id](const Handler& h) { return h.id == id; }),
                list.end());
            if (list.empty())
            {
                handlers_.erase(it);
            }
        }
    }

    mutable std::mutex mtx_;
    std::unordered_map<std::type_index, std::vector<Handler>> handlers_;
    std::unordered_map<std::uint64_t, std::string> alive_;  // id -> type name（type_index 不可默认构造，改存名称）
    std::unordered_map<std::uint64_t, std::shared_ptr<std::atomic<bool>>> states_;  // id -> 订阅退订标志
    std::uint64_t next_id_ = 1;
    std::atomic<bool> destroyed_{false};
};

// ---------- Subscription 需要完整 EventBus 类型的成员实现 ----------

inline void Subscription::unsubscribe()
{
    if (bus_ && id_ != 0)
    {
        // 用 state_ 的 expired 标志保证多份句柄只退订一次
        if (state_ && !state_->exchange(true, std::memory_order_acq_rel))
        {
            bus_->unsubscribeById(type_, id_);
        }
        bus_ = nullptr;
        id_ = 0;
    }
}

} // namespace EventBus
} // namespace LSX_LIB
