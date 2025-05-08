# LIBLSX 工具库 Thread 模块使用说明

## 模块概述

LIBLSX 工具库的 Thread 模块旨在简化 C++ 多线程编程的复杂性，提供一套易于使用的线程管理工具。该模块包括以下几个核心组件：

* **ThreadWrapper**：用于封装和管理单个线程，提供线程的生命周期控制（启动、停止、暂停、恢复）和状态跟踪功能。
* **Scheduler**：基于 ThreadWrapper 实现的任务调度器，用于管理一次性延迟任务和周期性任务。
* **ThreadPool**：线程池实现类，继承自 IThreadPool 接口，用于管理多个工作线程并执行任务队列中的任务。
* **IThreadPool** 和 **ICommunicator**：抽象接口类，分别为线程池和线程间通信定义了标准接口，以便未来扩展。
* **ThreadState**：枚举类，定义了线程在其生命周期中可能处于的不同状态。

## ThreadWrapper 使用说明

### 功能描述

ThreadWrapper 类封装了 std::thread，提供线程的生命周期管理（启动、停止、暂停、恢复）和状态跟踪功能。支持绑定任意可调用对象作为线程任务，并使用原子标志和条件变量实现线程控制。

### 任务绑定方式和支持类型

ThreadWrapper 支持以下四种任务绑定方式：

1. **Lambda 表达式 (Lambda Function)**：适合简单的、不需要复用的任务逻辑。
2. **自由函数 (Free Function)**：适用于独立的函数，包括带参数和不带参数的情况。
3. **类的成员函数 (Member Function)**：允许将类的成员函数作为任务。
4. **函数对象 (Function Object)**：支持将定义了 `operator()` 的对象作为任务。

### 无返回值任务绑定示例

#### 1. Lambda 表达式

```cpp
#include "LSX_LIB/Thread/ThreadWrapper.h"
#include <iostream>
#include <chrono>
#include <thread>

int main() {
    LSX_LIB::Thread::ThreadWrapper tw;

    // 绑定一个无参 lambda
    tw.setTask([]() {
        std::cout << "无参 Lambda 任务执行中..." << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    });
    tw.start();
    tw.stop();

    // 绑定一个带参 lambda
    tw.setTask([](int x, std::string msg) {
        std::cout << "带参 Lambda 任务执行，参数 x=" << x << ", msg=" << msg << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }, 42, "Hello Lambda");
    tw.start();
    tw.stop();

    return 0;
}
```

#### 2. 自由函数

```cpp
#include "LSX_LIB/Thread/ThreadWrapper.h"
#include <iostream>
#include <chrono>
#include <thread>

// 无参自由函数
void free_task() {
    std::cout << "无参自由函数任务执行中..." << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
}

// 带参自由函数
void free_task_with_args(int x, std::string msg) {
    std::cout << "带参自由函数任务执行，参数 x=" << x << ", msg=" << msg << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
}

int main() {
    LSX_LIB::Thread::ThreadWrapper tw;

    // 绑定无参自由函数
    tw.setTask(free_task);
    tw.start();
    tw.stop();

    // 绑定带参自由函数
    tw.setTask(free_task_with_args, 42, "Hello Free Function");
    tw.start();
    tw.stop();

    return 0;
}
```

#### 3. 类的成员函数

```cpp
#include "LSX_LIB/Thread/ThreadWrapper.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <string>

class Worker {
public:
    void member_task(int x) {
        std::cout << "成员函数任务执行，参数 x=" << x << ", name=" << name_ << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    std::string name_;
};

int main() {
    LSX_LIB::Thread::ThreadWrapper tw;
    Worker worker;
    worker.name_ = "WorkerInstance";

    // 绑定成员函数
    tw.setTask(&Worker::member_task, &worker, 42);
    tw.start();
    tw.stop();

    return 0;
}
```

#### 4. 函数对象

```cpp
#include "LSX_LIB/Thread/ThreadWrapper.h"
#include <iostream>
#include <chrono>
#include <thread>

class TaskObject {
public:
    void operator()() const {
        std::cout << "函数对象任务执行（无参）..." << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    void operator()(int x) const {
        std::cout << "函数对象任务执行，参数 x=" << x << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
};

int main() {
    LSX_LIB::Thread::ThreadWrapper tw;
    TaskObject task_obj;

    // 绑定无参函数对象
    tw.setTask(task_obj);
    tw.start();
    tw.stop();

    // 绑定带参函数对象
    tw.setTask(task_obj, 42);
    tw.start();
    tw.stop();

    return 0;
}
```

### 有返回值任务的处理方式

ThreadWrapper 本身不直接支持返回值任务，但可以通过以下方式实现：

1. **共享变量**：任务函数修改一个共享变量（使用互斥锁或原子操作保证线程安全）。
2. **回调函数**：任务完成时调用回调函数并传递结果。
3. **std::future 和 std::promise**：使用标准库的异步机制传递结果。

#### 使用 std::future 和 std::promise 示例

```cpp
#include "LSX_LIB/Thread/ThreadWrapper.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <future>

int main() {
    std::promise<int> promise; // 用于传递返回值
    std::future<int> future = promise.get_future(); // 在主线程中获取 future

    LSX_LIB::Thread::ThreadWrapper tw;

    // 绑定任务，使用 lambda 捕获 promise
    tw.setTask([&promise]() {
        std::cout << "任务执行中，计算结果..." << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        int result = 42; // 假设任务计算结果为 42
        promise.set_value(result); // 设置返回值
    });

    tw.start();

    // 在主线程中等待任务完成并获取结果
    try {
        int result = future.get(); // 阻塞等待结果
        std::cout << "获取到任务返回值: " << result << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "获取任务结果失败: " << e.what() << std::endl;
    }

    tw.stop();

    return 0;
}
```

## Scheduler 使用说明

### 功能描述

Scheduler 类利用 ThreadWrapper 在后台线程中执行一次性或周期性任务。它允许用户提交一个函数，指定延迟时间或周期间隔，然后在独立的线程中执行这些任务。Scheduler 负责创建和管理这些任务线程的生命周期，并在析构或调用 shutdown 时停止所有正在运行的任务。

### 任务提交方式和支持类型

Scheduler 支持以下任务提交方式：

1. **Lambda 表达式**：适合简单的任务逻辑。
2. **自由函数**：适用于独立的任务函数。
3. **类的成员函数**：允许将类的成员函数作为任务。
4. **函数对象**：支持将定义了 `operator()` 的对象作为任务。

### 使用示例

#### 一次性延迟任务

```cpp
#include "LSX_LIB/Thread/Scheduler.h"
#include <iostream>
#include <chrono>
#include <thread>

// 自由函数任务
void once_task() {
    std::cout << "一次性任务执行于线程 " << std::this_thread::get_id() << std::endl;
}

int main() {
    LSX_LIB::Thread::Scheduler scheduler;

    std::cout << "调度一个一次性任务，延迟 2 秒执行..." << std::endl;
    scheduler.scheduleOnce(2000, once_task); // 提交自由函数

    // 提交带参 lambda
    scheduler.scheduleOnce(2000, [](int x, std::string msg) {
        std::cout << "带参 lambda 任务执行，x=" << x << ", msg=" << msg << std::endl;
    }, 42, "Hello Once");

    std::this_thread::sleep_for(std::chrono::seconds(3));
    scheduler.shutdown();

    return 0;
}
```

#### 周期性任务

```cpp
#include "LSX_LIB/Thread/Scheduler.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <atomic>

// 自由函数任务
void periodic_task() {
    std::cout << "周期性任务执行于线程 " << std::this_thread::get_id() << std::endl;
}

int main() {
    LSX_LIB::Thread::Scheduler scheduler;
    std::atomic<int> counter{0};
    const int max_ticks = 5;

    std::cout << "调度一个周期性任务，每秒执行一次..." << std::endl;

    // 提交带状态的 lambda
    scheduler.schedulePeriodic(1000, [&counter, max_ticks]() {
        int current = ++counter;
        std::cout << "周期性任务执行，第 " << current << " 次" << std::endl;
        if (current >= max_ticks) {
            std::cout << "达到最大执行次数，任务将停止" << std::endl;
        }
    });

    std::this_thread::sleep_for(std::chrono::seconds(6)); // 足够的时间让任务执行多次
    scheduler.shutdown();

    return 0;
}
```

## ThreadPool 使用说明

### 功能描述

ThreadPool 类继承自 IThreadPool 接口，用于管理多个工作线程并执行任务队列中的任务。它允许多个任务并发执行，并支持任务的异步提交和结果获取。

### 任务提交方式和支持类型

ThreadPool 支持以下任务提交方式：

1. **无返回值无参数任务**：通过 `enqueue(std::function<void()>)` 提交。
2. **带返回值和参数任务**：通过模板函数 `enqueue(F&&, Args&&...)` 提交，支持任意可调用对象。

### 使用示例

#### 提交无返回值任务

```cpp
#include "LSX_LIB/Thread/ThreadPool.h"
#include <iostream>
#include <unistd.h>

void simple_task() {
    pthread_t tid = pthread_self();
    std::cout << "Simple Task started with ID: " << tid << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::cout << "Simple Task finished." << std::endl;
}

int main() {
    LSX_LIB::Thread::ThreadPool pool(2); // 创建一个包含 2 个线程的线程池

    pool.enqueue(simple_task);
    pool.enqueue(simple_task);

    return 0;
}
```

#### 提交带返回值任务

```cpp
#include "LSX_LIB/Thread/ThreadPool.h"
#include <iostream>
#include <future>
#include <string>

// 带返回值和参数的任务函数
int complex_task(int x, std::string msg) {
    std::cout << "Complex Task received x=" << x << ", msg=" << msg << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    return x * 2; // 返回计算结果
}

int main() {
    LSX_LIB::Thread::ThreadPool pool(2);

    // 提交带返回值任务并获取 future
    auto future1 = pool.enqueue(complex_task, 42, "Calculate Double");
    auto future2 = pool.enqueue([](std::string msg) { return msg + " from lambda"; }, "Hello");

    // 获取任务结果
    try {
        std::cout << "Result of complex_task: " << future1.get() << std::endl;
        std::cout << "Result of lambda task: " << future2.get() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "获取任务结果失败: " << e.what() << std::endl;
    }

    return 0;
}
```

## 注意事项

1. **线程安全**：ThreadWrapper 和 Scheduler 内部操作是线程安全的，但任务函数中访问共享数据时需自行处理同步。
2. **异常处理**：任务函数中的异常需自行捕获处理，未捕获的异常可能导致线程终止。
3. **资源管理**：使用完毕后需调用 `stop()` 或 `shutdown()` 确保线程资源正确释放。
4. **任务返回值**：ThreadWrapper 不直接支持返回值任务，需通过共享变量、回调或标准库异步机制实现。

## 总结

LIBLSX 工具库的 Thread 模块提供了一套功能完善的多线程编程工具，支持多种任务绑定方式和灵活的线程控制机制。通过 ThreadWrapper、Scheduler 和 ThreadPool，开发者可以高效地管理线程生命周期、调度任务并利用多核资源。
