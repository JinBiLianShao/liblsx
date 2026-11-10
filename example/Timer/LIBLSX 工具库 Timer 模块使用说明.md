# LIBLSX 工具库 Timer 模块使用说明

---

## 目录

1. [模块概述](#模块概述)
2. [模块架构](#模块架构)
3. [构建与引入](#构建与引入)
4. [TimerWheel 使用说明](#timerwheel-使用说明)
5. [Stopwatch 使用说明](#stopwatch-使用说明)
6. [Deadline 使用说明](#deadline-使用说明)
7. [与 Scheduler 的选型对比](#与-scheduler-的选型对比)
8. [常见错误用法与规避](#常见错误用法与规避)

---

## 模块概述

Timer 模块提供基于**单工作线程**的多定时器管理，以及计时/超时工具。与 Thread 模块
的 Scheduler（一任务一线程）相比，TimerWheel 用一个线程驱动任意数量的定时器，
适合大规模定时需求（嵌入式场景下尤其重要）。

核心特性：

* **单线程驱动多定时器**：内部最小堆按最近到期时间休眠，新增/取消随时打断重排。
* **一次性 + 周期性**：scheduleOnce 触发一次，schedulePeriodic 按间隔循环。
* **回调内操作安全**：回调中可 cancel 自己/其它定时器、可再次调度，不会死锁。
* **RAII 生命周期**：析构自动 shutdown，不会挂起或泄漏线程。
* **线程安全**：所有公开方法可多线程并发调用。

---

## 模块架构

```
Include/Timer/
└── Timer.h            # 全部声明（TimerWheel/TimerId/Stopwatch/Deadline）
src/Timer/
└── Timer.cpp          # TimerWheel 实现（Stopwatch/Deadline 为头文件内联）
```

| 组件 | 类型 | 职责 |
|------|------|------|
| `TimerWheel` | 类 | 定时器管理器：调度、触发、取消 |
| `TimerId` | 结构体 | 定时器句柄：cancel/isActive 的凭据 |
| `Stopwatch` | 类 | 计时工具：流逝时间测量 |
| `Deadline` | 类 | 超时判断：期限检查与剩余时间 |

命名空间：`LSX_LIB::Timer`。

---

## 构建与引入

### 通过 CMake（推荐）

```cmake
# TARGET_TYPE=both/static/shared 构建出的库均包含 Timer 模块
# 链接 liblsx 静态库或动态库即可
target_link_libraries(your_app PRIVATE lsx_static)
```

### 手动集成

Timer 模块只需两个文件：`Include/Timer/Timer.h` 与 `src/Timer/Timer.cpp`，
把 cpp 加入编译、`Include/` 加入头文件搜索路径即可，无第三方依赖。

### 链接要求

TimerWheel 内部使用 `std::thread`，Linux 下需要链接线程库：

```cmake
target_link_libraries(your_app PRIVATE pthread)
```

---

## TimerWheel 使用说明

### 生命周期

```cpp
LSX_LIB::Timer::TimerWheel timers;
timers.start();                      // 启动工作线程（幂等，重复调用无害）
// ... scheduleOnce / schedulePeriodic / cancel ...
timers.shutdown();                   // 停止线程并清空全部待触发定时器（幂等）
```

* start() 之后才能 schedule；shutdown 之后 schedule 返回**无效句柄**。
* 析构自动调用 shutdown——忘记 shutdown 也不会泄漏线程。
* shutdown 会**丢弃全部未触发的定时器**，需要"停机前跑完"的场景请自行等待。

### 一次性定时器

```cpp
#include "Timer/Timer.h"

LSX_LIB::Timer::TimerWheel timers;
timers.start();

auto id = timers.scheduleOnce(500, []() {          // 500ms 后触发一次
    std::cout << "timeout!" << std::endl;
});

// 触发前可取消
if (timers.cancel(id)) {
    std::cout << "取消成功" << std::endl;
}
// 触发后再 cancel 返回 false；isActive(id) 同步变 false
```

### 周期性定时器

```cpp
std::atomic<int> ticks{0};

auto id = timers.schedulePeriodic(100, [&ticks]() {   // 每 100ms 一次
    ++ticks;
});
// ... 运行期间每个间隔触发一次 ...
timers.cancel(id);                                     // 停止循环
```

周期语义：

* 间隔从**上次触发时刻**起算（非漂移对齐）。回调耗时长于间隔时，下一次
  会在回调结束后立即触发（不补触发错过的次数）。
* 回调里 cancel 自己：下一个间隔**不会**再触发（下一次到期在回调执行前
  已登记，取消后立即失效）。

### 回调内操作（全部安全）

```cpp
// 1. 回调内取消自己（防抖/单次重试场景常用）
LSX_LIB::Timer::TimerId self;
self = timers.schedulePeriodic(50, [&]() {
    if (done()) timers.cancel(self);   // 取消后不再触发
});

// 2. 回调内调度新定时器（链式延迟）
timers.scheduleOnce(100, [&]() {
    timers.scheduleOnce(200, []() { /* 两段延迟 */ });
});

// 3. 回调内发布事件（与 EventBus 组合）
timers.scheduleOnce(1000, [&]() {
    bus.publish(SensorTimeout{id});
});
```

### 并发使用

所有公开方法线程安全，典型并发形态：

```cpp
// 线程 A：注册定时器
timers.scheduleOnce(1000, task_a);

// 线程 B：取消某个定时器
timers.cancel(id_from_a);

// 线程 C：查询
std::size_t n = timers.size();       // 当前待触发数量
bool live = timers.isActive(id);     // 某定时器是否待触发
```

### API 参考（TimerWheel）

| 方法 | 说明 | 返回 |
|------|------|------|
| `void start()` | 启动工作线程（幂等） | — |
| `void shutdown()` | 停止线程并清空待触发定时器（幂等） | — |
| `TimerId scheduleOnce(int64 delay_ms, Callback)` | 一次性定时器 | 句柄（失败/已 shutdown 返回无效） |
| `TimerId schedulePeriodic(int64 interval_ms, Callback)` | 周期定时器 | 句柄（同上） |
| `bool cancel(TimerId)` | 取消（幂等；已触发返回 false） | 是否取消成功 |
| `bool isActive(TimerId)` | 是否待触发 | — |
| `size_t size()` | 待触发数量 | — |
| `bool isRunning()` | 工作线程是否在运行 | — |

参数校验：负 delay、非正 interval、空回调均返回无效句柄，不会抛异常。

---

## Stopwatch 使用说明

基于 `std::chrono::steady_clock`（单调时钟，不受系统时间跳变影响）。

```cpp
LSX_LIB::Timer::Stopwatch sw;        // 构造即开始计时

auto ms = sw.elapsedMs();            // 已流逝毫秒（整数）
auto us = sw.elapsedUs();            // 已流逝微秒（整数）
auto s  = sw.elapsedSeconds();       // 已流逝秒（浮点，double）

sw.restart();                        // 重新从现在开始计时
```

典型用途：测量一段代码的耗时、限流窗口、重试间隔。

```cpp
LSX_LIB::Timer::Stopwatch sw;
do_expensive_work();
std::cout << "耗时 " << sw.elapsedMs() << "ms" << std::endl;
```

---

## Deadline 使用说明

以构造时刻为起点的超时判断，常用于循环里检查"还有没有时间"。

```cpp
LSX_LIB::Timer::Deadline d(5000);    // 5 秒期限

while (!d.expired()) {
    if (!process_one_item()) break;
}
// remainingMs() 返回剩余毫秒（超时后为 0）
```

**永不超时语义**：timeout_ms <= 0（含 0 和负数）表示永不超时——

* expired() 恒为 false
* remainingMs() 恒为 -1

适合"可配置超时，0 表示不限"的接口约定。

---

## 与 Scheduler 的选型对比

| 维度 | TimerWheel（本模块） | Scheduler（Thread 模块） |
|------|---------------------|--------------------------|
| 线程占用 | **1 个工作线程**服务全部定时器 | 每个任务一个线程 |
| 定时器规模 | 成百上千无压力 | 十几个即有明显开销 |
| 精度 | 毫秒级，统一排序调度 | 毫秒级，独立线程独立计时 |
| 回调阻塞影响 | 阻塞同轮其它定时器 | 只影响自己的线程 |
| 暂停/恢复 | 不支持 | 支持（ThreadWrapper 提供） |
| 适用场景 | 大量短回调定时（心跳、轮询、防抖） | 少量长任务、需要生命周期控制 |

**经验法则**：默认用 TimerWheel；只有回调必须长时间阻塞（如阻塞式 IO 循环）
或需要暂停/恢复时才用 Scheduler。

---

## 常见错误用法与规避

### 1. 回调里做阻塞操作

```cpp
// 错误：阻塞回调会拖延同轮其它定时器
timers.schedulePeriodic(100, []() {
    auto data = blocking_recv();   // 可能阻塞数秒
    process(data);
});

// 正确：回调只投递，重活交给线程池
timers.schedulePeriodic(100, [&pool]() {
    pool.enqueue([]() { blocking_recv_and_process(); });
});
```

### 2. 用 getState() == STOPPED 判断任务退出

参见 Thread 模块文档的同名章节——定时器回调内如果操作 ThreadWrapper，
退出条件请用 isStopRequested()。

### 3. 期望 shutdown 后回调再触发

shutdown 清空全部待触发定时器。需要"关闭前完成最后一次"的逻辑请在
shutdown 前自行等待相应定时器触发（配合 isActive 轮询或事件通知）。

### 4. 在回调里同步等待另一个定时器

两个定时器共用一个工作线程，互相等待会死锁。跨定时器等待请用
EventBus + 线程池，或把逻辑合并进一个回调。
