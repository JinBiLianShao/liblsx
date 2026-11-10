# LIBLSX 工具库 EventBus 模块使用说明

---

## 目录

1. [模块概述](#模块概述)
2. [模块架构](#模块架构)
3. [核心概念](#核心概念)
4. [快速上手](#快速上手)
5. [EventBus API 参考](#eventbus-api-参考)
6. [Subscription 句柄语义](#subscription-句柄语义)
7. [线程模型与并发](#线程模型与并发)
8. [与其它模块的组合模式](#与其它模块的组合模式)
9. [常见错误用法与规避](#常见错误用法与规避)

---

## 模块概述

EventBus 提供类型安全的发布-订阅（pub/sub）机制，用于模块间解耦：发布者不需要
知道谁在消费，订阅者按事件类型注册处理回调。

核心特性：

* **类型即路由键**：事件类型（任意可拷贝 C++ 类型）决定分发目标，编译期类型安全。
* **RAII 订阅句柄**：Subscription 析构自动退订，杜绝忘记退订导致的悬垂回调。
* **回调内安全**：回调中可嵌套 publish、subscribe、unsubscribe（含退订自己）。
* **线程安全**：publish/subscribe/unsubscribe 均可多线程并发调用。
* **header-only**：整个模块只有一个头文件，直接 include 即可使用。

---

## 模块架构

```
Include/EventBus/
├── EventBus.h         # EventBus + Subscription 全部实现（header-only）
└── lsx_eventbus.h     # 便利头文件（等价于 include EventBus.h）
```

命名空间：`LSX_LIB::EventBus`。

---

## 核心概念

### 事件类型

任意**可拷贝**的 C++ 类型都可作为事件。推荐为每类事件定义专门的结构体：

```cpp
struct SensorData   { int id; double value; };
struct ConfigChanged{ std::string key; };
struct ShutdownRequested {};
```

事件类型本身即路由键——subscribe<T> 与 publish(T) 按编译期类型精确匹配，
不同类型的事件互不干扰，无需字符串约定或枚举注册。

### 订阅句柄（Subscription）

subscribe 返回一个 Subscription 句柄，它是退订的唯一凭据：

* **RAII**：句柄析构自动退订，离开作用域即安全解除
* **手动退订**：sub.unsubscribe()，幂等，可随时调用
* **拷贝共享**：拷贝出的句柄与原句柄共享退订标志，任意一份退订全部失效
* **移动接续**：移动后的新句柄接管原订阅
* **isValid()**：查询是否仍有效（未退订且总线存活）

---

## 快速上手

```cpp
#include "EventBus/EventBus.h"

// 1. 定义事件类型（struct 字段即事件数据）
struct SensorData { int id; double value; };

LSX_LIB::EventBus::EventBus bus;

// 2. 订阅：注册处理回调
auto sub = bus.subscribe<SensorData>([](const SensorData& e) {
    std::cout << "传感器 " << e.id << " 数值 " << e.value << std::endl;
});

// 3. 发布：在调用者线程同步分发给全部订阅者
bus.publish(SensorData{1, 3.14});

// 4. 退订（二选一）
sub.unsubscribe();   // 手动退订
// 或让 sub 离开作用域自动退订
```

---

## EventBus API 参考

| 方法 | 说明 |
|------|------|
| `template<class T> Subscription subscribe(std::function<void(const T&)>)` | 订阅 T 类型事件，返回句柄 |
| `template<class T> void publish(const T& event)` | 发布事件，同步分发给全部 T 订阅者 |
| `template<class T> size_t subscriberCount()` | 当前 T 类型的订阅者数量 |

无订阅者时 publish 是安全的空操作，不会分配或报错。

---

## Subscription 句柄语义

```cpp
auto sub = bus.subscribe<EvClick>(handler);

// 手动退订（幂等）
sub.unsubscribe();

// 有效性与总线状态联动
if (sub.isValid()) { /* 仍订阅中 */ }

// 拷贝：共享退订标志，任意一份退订即全部失效
auto copy = sub;
copy.unsubscribe();     // sub 也随之失效

// 移动：新句柄接管订阅
auto moved = std::move(sub);
```

**总线销毁保护**：EventBus 析构会使全部句柄自动失效。之后对句柄调用
unsubscribe/isValid 均安全（不会回访已销毁的总线）——这是测试覆盖过的边界。

---

## 线程模型与并发

### 分发语义

publish 在**调用者线程**同步分发给全部订阅者——回调与 publish 调用方
在同一线程执行。这带来两个推论：

* 订阅回调本身**不需要**是线程安全的，只要同一订阅者的回调不会从多个
  线程并发触发（同步分发天然满足）
* publish 的耗时 = 全部订阅回调耗时之和，重回调会拖慢发布方

### 多线程并发发布

多个线程同时 publish 是安全的，回调会分别在各发布线程中执行：

```cpp
// 4 个线程并发发布，订阅者累计收到全部事件（每线程独立串行）
std::vector<std::thread> publishers;
for (int t = 0; t < 4; ++t) {
    publishers.emplace_back([&bus]() {
        for (int i = 0; i < 100; ++i) bus.publish(EvData{"ab"});
    });
}
```

### 需要异步分发时

EventBus 本身不提供异步队列，推荐与 ThreadPool 组合——订阅回调只做投递：

```cpp
bus.subscribe<SensorData>([&pool](const SensorData& e) {
    pool.enqueue([e]() { heavy_process(e); });   // 按值捕获，投递到线程池
});
```

---

## 与其它模块的组合模式

### EventBus + TimerWheel：防抖 / 延迟响应

```cpp
// 配置高频变更时，停止变更 100ms 后才真正重载（防抖）
bus.subscribe<ConfigChanged>([&timers](const ConfigChanged&) {
    static LSX_LIB::Timer::TimerId pending;
    timers.cancel(pending);
    pending = timers.scheduleOnce(100, []() { reload_config(); });
});
```

### EventBus + ThreadPool：异步事件处理

```cpp
// 事件投递线程池，发布方不被重回调阻塞
bus.subscribe<FrameArrived>([&pool](const FrameArrived& f) {
    pool.enqueue([f]() { decode_and_render(f); });
});
```

### EventBus + Logger：事件审计

```cpp
// 统一事件日志切面：所有事件类型都可包一层记录
bus.subscribe<SensorData>([&logger](const SensorData& e) {
    LSX_LOG_INFO(logger, "sensor " + std::to_string(e.id) + " = " + std::to_string(e.value));
});
```

---

## 常见错误用法与规避

### 1. 在订阅回调里做阻塞操作

```cpp
// 会阻塞发布方线程（同步分发）——重活请投递线程池（见上文组合模式）
bus.subscribe<FrameArrived>([](const FrameArrived& f) {
    blocking_decode(f);   // 阻塞数秒，发布方跟着卡住
});
```

### 2. 用字符串或裸 int 做事件类型

```cpp
// 类型不承载语义，subscribe<int> 会接收所有 int 事件
bus.publish(42);

// 正确：为每类事件定义 struct
struct TemperatureAlert { int sensor_id; double value; };
```

### 3. 忘记保存 subscribe 返回的句柄

```cpp
// 错误：句柄立即析构 → 订阅立即退订 → 回调永远不会被调用
bus.subscribe<EvClick>(handler);

// 正确：保存句柄，按需管理生命周期
auto sub = bus.subscribe<EvClick>(handler);
```

### 4. 在总线销毁后仍期待回调触发

EventBus 析构后所有订阅失效且不再触发任何回调。生命周期上应保证
总线比全部订阅者活得久（成员声明顺序、单例析构顺序需留意）。
