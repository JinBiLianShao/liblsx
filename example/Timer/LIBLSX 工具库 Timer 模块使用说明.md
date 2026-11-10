# LIBLSX 工具库 Timer 模块使用说明

## 模块概述

Timer 模块提供基于单工作线程的多定时器管理，以及计时/超时工具。与 Thread 模块
的 Scheduler（一任务一线程）相比，TimerWheel 用一个线程驱动任意数量的定时器，
适合大规模定时需求（嵌入式场景下尤其重要）。

## 组件

* **TimerWheel**：定时器管理器。一次性（scheduleOnce）与周期性（schedulePeriodic）
  定时器，支持任意时刻取消（cancel）、回调内再次调度/取消自己。
* **TimerId**：定时器句柄，cancel/isActive 以此为凭据。
* **Stopwatch**：基于 steady_clock 的计时工具（ms/us/秒）。
* **Deadline**：超时判断工具（expired/remainingMs，0 或负值表示永不超时）。

## 快速上手

```cpp
#include "Timer/Timer.h"

LSX_LIB::Timer::TimerWheel timers;
timers.start();

// 一次性：500ms 后触发
auto id1 = timers.scheduleOnce(500, []() {
    // 触发时执行
});

// 周期性：每 100ms 一次，直至取消
auto id2 = timers.schedulePeriodic(100, []() {
    // 每次触发执行
});

// 取消定时器
timers.cancel(id2);

// 停止并清空全部定时器
timers.shutdown();
```

## 行为要点

* **回调执行线程**：全部在定时器工作线程中串行执行。回调内不要做阻塞操作，
  否则会拖延同一轮次的其它定时器。
* **回调内操作安全**：回调中可以调用 cancel / scheduleOnce / schedulePeriodic，
  包括取消回调自己的定时器（周期任务取消后不再重排下一次）。
* **shutdown 后**：待触发的定时器全部清空、不再触发；schedule 返回无效句柄。
* **析构**：TimerWheel 析构自动 shutdown，不会挂起。

## Stopwatch 与 Deadline

```cpp
LSX_LIB::Timer::Stopwatch sw;
auto ms = sw.elapsedMs();      // 毫秒
auto us = sw.elapsedUs();      // 微秒
auto s  = sw.elapsedSeconds(); // 秒（浮点）
sw.restart();                  // 重新计时

LSX_LIB::Timer::Deadline d(5000);  // 5 秒期限
if (d.expired()) { /* 超时 */ }
auto left = d.remainingMs();       // 剩余毫秒
```

## 与 Scheduler 的选择建议

| 场景 | 推荐 |
|------|------|
| 定时器数量多（几十个以上） | TimerWheel（单线程驱动） |
| 定时器少且回调较重 | Scheduler（每任务独立线程） |
| 需要暂停/恢复控制 | Scheduler + ThreadWrapper |
| 计时/超时判断 | Stopwatch / Deadline |
