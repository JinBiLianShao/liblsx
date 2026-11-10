# LIBLSX 工具库 EventBus 模块使用说明

## 模块概述

EventBus 提供类型安全的发布-订阅（pub/sub）机制，用于模块间解耦：发布者不需要
知道谁在消费，订阅者按事件类型注册处理回调。

## 核心概念

* **事件类型**：任意可拷贝的 C++ 类型（推荐自定义 struct），类型本身即路由键。
* **EventBus**：事件总线，publish 按类型分发。
* **Subscription**：订阅句柄，RAII 语义——析构自动退订；也支持手动 unsubscribe。

## 快速上手

```cpp
#include "EventBus/EventBus.h"

// 1. 定义事件类型（struct 字段即事件数据）
struct SensorData { int id; double value; };

LSX_LIB::EventBus::EventBus bus;

// 2. 订阅
auto sub = bus.subscribe<SensorData>([](const SensorData& e) {
    // 处理事件
});

// 3. 发布（在调用者线程同步分发给全部订阅者）
bus.publish(SensorData{1, 3.14});

// 4. 退订（二选一：手动退订 或 句柄析构自动退订）
sub.unsubscribe();
```

## 行为要点

* **分发语义**：publish 在调用者线程同步分发。需要异步请与 ThreadPool/
  TimerWheel 组合（回调中投递任务或定时器）。
* **回调内操作安全**：回调中可以 publish（嵌套发布）、subscribe、unsubscribe
  （含退订自己），总线在锁外执行回调保证不死锁。
* **句柄拷贝**：Subscription 拷贝共享同一退订标志，任意一份退订即全部失效；
  移动后的句柄接续原订阅。
* **总线销毁**：EventBus 析构使全部句柄失效；之后对句柄 unsubscribe/isValid
  均安全，不会回访已销毁的总线。
* **多线程**：publish/subscribe/unsubscribe 均线程安全，可多线程并发发布。

## 典型组合

```cpp
// 事件触发定时刷新（EventBus + TimerWheel 组合）
bus.subscribe<ConfigChanged>([&timers](const ConfigChanged&) {
    timers.scheduleOnce(100, []{ reload_config(); });  // 防抖：100ms 后执行
});
```
