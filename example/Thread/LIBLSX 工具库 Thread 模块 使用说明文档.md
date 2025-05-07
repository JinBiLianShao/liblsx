## LSX_LIB::Thread 库详细使用教程

本教程将深入讲解如何使用 LSX_LIB::Thread 库的核心组件 `ThreadWrapper` 和 `Scheduler`，包括如何绑定不同类型的任务以及进行更精细的线程控制。

### 1. 前提条件

* 在您的项目中导入 LSX_LIB::Thread 库。
* 您需要一个 C++11 或更高版本的编译器。

### 2. 使用 `LSX_LIB::Thread::ThreadWrapper` (线程生命周期管理)

`ThreadWrapper` 类用于封装一个独立的线程，提供对其生命周期的精细控制（启动、停止、暂停、恢复、重启）以及任务的绑定。

#### 2.1 绑定任务 (`setTask`)

`setTask` 方法是 `ThreadWrapper` 的核心，它允许您将任何**可调用对象**（Callable）及其参数绑定到线程上。可调用对象可以是：

* **Lambda 表达式 (Lambda Function)**
* **自由函数 (Free Function)**
* **类的成员函数 (Member Function)**
* **函数对象 (Function Object)**

`setTask` 内部使用 `std::bind` 来处理可调用对象和参数的绑定。

**示例 2.1.1: 绑定 Lambda 表达式**

这是最常见和方便的方式，尤其适用于任务逻辑简单且不需要在其他地方复用的场景。

```cpp
#include "LSX_LIB/Thread/ThreadWrapper.h"
#include <iostream>
#include <chrono>
#include <thread>

int main() {
    using namespace LSX_LIB::Thread;

    ThreadWrapper tw_lambda;

    // 绑定一个简单的 lambda 任务
    tw_lambda.setTask([](){
        std::cout << "任务：这是一个来自 Lambda 的任务。\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // 模拟工作
        std::cout << "任务：Lambda 任务执行完毕。\n";
    });

    std::cout << "启动 Lambda 任务线程...\n";
    tw_lambda.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(200)); // 等待任务完成
    tw_lambda.stop(); // 停止并等待线程退出
    std::cout << "Lambda 任务线程已停止。\n";

    return 0;
}
```

**示例 2.1.2: 绑定自由函数 (不带参数)**

您可以直接绑定一个全局函数或静态函数。

```cpp
#include "LSX_LIB/Thread/ThreadWrapper.h"
#include <iostream>
#include <chrono>
#include <thread>

// 一个不带参数的自由函数
void my_free_task() {
    std::cout << "任务：这是一个来自自由函数的任务。\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // 模拟工作
    std::cout << "任务：自由函数任务执行完毕。\n";
}

int main() {
    using namespace LSX_LIB::Thread;

    ThreadWrapper tw_free_func;

    // 绑定自由函数
    tw_free_func.setTask(my_free_task);

    std::cout << "启动自由函数任务线程...\n";
    tw_free_func.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(200)); // 等待任务完成
    tw_free_func.stop(); // 停止并等待线程退出
    std::cout << "自由函数任务线程已停止。\n";

    return 0;
}
```

**示例 2.1.3: 绑定自由函数 (带参数)**

使用 `setTask` 绑定带参数的自由函数时，参数会作为后续参数传递给 `setTask`，内部通过 `std::bind` 进行绑定。

```cpp
#include "LSX_LIB/Thread/ThreadWrapper.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <string>

// 一个带参数的自由函数
void my_free_task_with_args(int id, const std::string& name) {
    std::cout << "任务：ID = " << id << ", Name = \"" << name << "\" 的自由函数任务。\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // 模拟工作
    std::cout << "任务：带参数自由函数任务执行完毕。\n";
}

int main() {
    using namespace LSX_LIB::Thread;

    ThreadWrapper tw_free_func_args;

    // 绑定带参数的自由函数，参数直接跟在函数名后面
    tw_free_func_args.setTask(my_free_task_with_args, 101, "WorkerAlpha");

    std::cout << "启动带参数自由函数任务线程...\n";
    tw_free_func_args.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(200)); // 等待任务完成
    tw_free_func_args.stop(); // 停止并等待线程退出
    std::cout << "带参数自由函数任务线程已停止。\n";

    return 0;
}
```

**示例 2.1.4: 绑定类的成员函数**

绑定成员函数需要指定函数指针和对象实例（或对象指针）。

```cpp
#include "LSX_LIB/Thread/ThreadWrapper.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <string>

class MyWorker {
public:
    MyWorker(const std::string& name) : name_(name) {}

    // 成员函数作为任务
    void process_task(int data) {
        std::cout << "任务：Worker \"" << name_ << "\" 正在处理数据: " << data << "。\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // 模拟工作
        std::cout << "任务：Worker \"" << name_ << "\" 处理数据完成。\n";
    }

private:
    std::string name_;
};

int main() {
    using namespace LSX_LIB::Thread;

    MyWorker worker_instance("Beta");
    ThreadWrapper tw_member_func;

    // 绑定成员函数：第一个参数是成员函数指针，第二个参数是对象指针
    tw_member_func.setTask(&MyWorker::process_task, &worker_instance, 202);

    std::cout << "启动成员函数任务线程...\n";
    tw_member_func.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(200)); // 等待任务完成
    tw_member_func.stop(); // 停止并等待线程退出
    std::cout << "成员函数任务线程已停止。\n";

    return 0;
}
```

#### 2.2 控制线程生命周期

`ThreadWrapper` 提供了一系列方法来控制线程的执行状态：

* `start(bool detached = false)`: 启动线程执行绑定的任务。如果 `detached` 为 `true`，线程将分离，您无法再使用 `join()` 等待它完成；如果为 `false` (默认)，线程是可 join 的。
* `stop()`: 请求线程停止并等待其完成 (`join`)。这是安全停止线程并清理资源的推荐方法。它会设置内部停止标志并唤醒可能处于暂停或等待状态的线程。
* `pause()`: 请求线程暂停。实际暂停发生在 `threadFunc` 内部检测到 `pauseFlag_` 时通过条件变量等待。
* `resume()`: 恢复暂停的线程。它清除内部暂停标志并通知条件变量。
* `restart(bool detached = false)`: 先调用 `stop()` 停止当前线程，然后调用 `start()` 启动新线程。**注意：** 当前实现中，`stop()` 会清理已绑定的任务 (`task_ = nullptr`)，因此调用 `restart()` 后，您通常需要**再次调用 `setTask()`** 才能让新线程执行任务。

**示例 2.2.1: 暂停与恢复**

这个示例需要一个会持续运行（比如循环）的任务来观察暂停效果。由于 `ThreadWrapper` 的默认任务执行模式是单次，我们需要一个在任务 lambda 内部包含循环和检查标志的例子来演示暂停。

```cpp
#include "LSX_LIB/Thread/ThreadWrapper.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <atomic> // 使用原子标志位

int main() {
    using namespace LSX_LIB::Thread;

    ThreadWrapper tw_control;
    std::atomic<bool> running{true}; // 用于控制任务内部循环

    tw_control.setTask([&](){
        int count = 0;
        // 任务内部循环，同时检查外部标志和 ThreadWrapper 的状态
        while(running.load() && tw_control.getState() != ThreadState::STOPPED) {
             // ThreadWrapper 的 threadFunc 会在 task_() 调用前处理暂停
             // 所以这里只需要关注 stop 标志或 running 标志
             std::cout << "任务：运行中，计数 " << ++count << "\n";
             std::this_thread::sleep_for(std::chrono::milliseconds(200));

             // 在任务内部主动检查暂停状态并等待 (可选，ThreadWrapper 也会处理)
             // 如果任务内部有耗时操作且不想 ThreadWrapper 等待太久，可以在这里主动检查
             // while(tw_control.getState() == ThreadState::PAUSED) {
             //    std::cout << "任务：已暂停...\n";
             //    std::this_thread::sleep_for(std::chrono::milliseconds(50));
             // }
        }
        std::cout << "任务：循环退出。\n";
    });

    std::cout << "启动控制线程...\n";
    tw_control.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(500)); // 运行一段时间

    std::cout << "请求暂停...\n";
    tw_control.pause();
    std::cout << "线程状态: " << static_cast<int>(tw_control.getState()) << std::endl; // 应该显示 PAUSED
    std::this_thread::sleep_for(std::chrono::milliseconds(1000)); // 观察暂停效果

    std::cout << "请求恢复...\n";
    tw_control.resume();
    std::cout << "线程状态: " << static_cast<int>(tw_control.getState()) << std::endl; // 应该显示 RUNNING
    std::this_thread::sleep_for(std::chrono::milliseconds(1000)); // 恢复运行一段时间

    std::cout << "请求停止...\n";
    running.store(false); // 通知任务循环退出
    tw_control.stop(); // 停止并等待线程退出
    std::cout << "控制线程已停止。\n";

    return 0;
}
```
*注意：上述暂停/恢复示例依赖于任务内部是循环，并且 `ThreadWrapper` 的 `threadFunc` 在调用 `task_()` 前处理了 `pauseFlag_`。如果任务本身是耗时但不含循环的单次调用，`pause()` 的效果可能不明显，因为它只在进入或退出 `task_()` 时检查状态。*

#### 2.3 获取线程状态 (`getState`)

随时可以使用 `getState()` 方法获取线程的当前状态。

```cpp
#include "LSX_LIB/Thread/ThreadWrapper.h"
#include <iostream>
#include <chrono>
#include <thread>

int main() {
    using namespace LSX_LIB::Thread;

    ThreadWrapper tw;
    std::cout << "初始状态: " << static_cast<int>(tw.getState()) << std::endl; // 应该显示 INIT

    tw.setTask([](){
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    });
    std::cout << "设置任务后状态: " << static_cast<int>(tw.getState()) << std::endl; // 应该显示 INIT

    tw.start();
    std::cout << "启动后状态: " << static_cast<int>(tw.getState()) << std::endl;   // 应该显示 RUNNING

    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // 运行一段时间

    tw.stop();
    std::cout << "停止后状态: " << static_cast<int>(tw.getState()) << std::endl;    // 应该显示 STOPPED

    return 0;
}
```

### 3. 使用 `LSX_LIB::Thread::Scheduler` (任务调度)

`Scheduler` 类利用 `ThreadWrapper` 在后台线程中执行一次性或周期性任务。

#### 3.1 一次性延迟任务 (`scheduleOnce`)

在指定延迟后执行任务一次。

```cpp
#include "LSX_LIB/Thread/Scheduler.h"
#include <iostream>
#include <chrono>
#include <thread>

int main() {
    using namespace LSX_LIB::Thread;

    Scheduler sched;

    std::cout << "主线程: 安排一个 1 秒后执行的任务...\n";
    sched.scheduleOnce(1000, [](){
        std::cout << "任务：一次性延迟任务执行了！\n";
    });

    // 主线程需要保持运行，否则调度器及其线程会被销毁
    std::cout << "主线程: 等待 2 秒...\n";
    std::this_thread::sleep_for(std::chrono::seconds(2));

    std::cout << "主线程: 调用 Scheduler::shutdown()...\n";
    sched.shutdown(); // 停止并清理所有调度任务
    std::cout << "主线程: Scheduler 已关闭。\n";

    return 0;
}
```

#### 3.2 周期性任务 (`schedulePeriodic`)

每隔指定间隔重复执行任务。任务的重复执行逻辑包含在您提供的 lambda/function 中。

```cpp
#include "LSX_LIB/Thread/Scheduler.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <atomic> // 用于示例中的计数和退出条件

int main() {
    using namespace LSX_LIB::Thread;

    Scheduler sched;
    std::atomic<int> counter{0};
    int max_ticks = 5;

    std::cout << "主线程: 安排一个每 500ms 执行的周期性任务...\n";
    sched.schedulePeriodic(500, [&](){ // 注意 lambda 捕获 counter
        int current_count = ++counter;
        std::cout << "任务：周期性任务执行，第 " << current_count << " 次。\n";

        if (current_count >= max_ticks) {
            std::cout << "任务：达到最大次数，请求停止调度器。\n";
            // 在实际应用中，任务不应直接调用 shutdown，而是设置标志通知主线程
            // 这里为了示例简单直接演示效果
            // 主线程在 sleep 后会调用 shutdown
        }
    });

    // 主线程等待足够长的时间让周期任务执行多次
    std::cout << "主线程: 等待 3 秒让任务执行...\n";
    std::this_thread::sleep_for(std::chrono::seconds(3));

    std::cout << "主线程: 调用 Scheduler::shutdown()...\n";
    sched.shutdown(); // 停止并清理所有调度任务 (包括周期任务)
    std::cout << "主线程: Scheduler 已关闭。\n";

    return 0;
}
```
*注意：周期性任务的 lambda 中内嵌了循环。为了让 Scheduler 的 `shutdown()` 能够停止这个任务，lambda 内部的循环**必须**检查 `ThreadWrapper` 的状态（或者任务自身的一个停止标志），并在收到停止信号时退出循环。Scheduler 的实现中，周期任务的 lambda 会检查 `tw->getState() != ThreadState::STOPPED` 来决定是否继续循环，这使得 `shutdown()` 通过调用 `tw->stop()` 来终止周期任务成为可能。*

#### 3.3 关闭调度器 (`shutdown`)

调用 `shutdown()` 会迭代停止所有当前由该 `Scheduler` 管理的任务线程。对于非分离的线程（如 `schedulePeriodic` 默认创建的线程），`shutdown` 会等待它们 `join` 完成。对于分离的线程（如 `scheduleOnce` 默认创建的线程），`shutdown` 仅设置停止标志，`join` 将无效，线程会在任务完成后自行结束。

### 4. 传递参数给任务 (总结)

无论是通过 `ThreadWrapper::setTask` 还是 `Scheduler::scheduleOnce`/`schedulePeriodic` 绑定任务，传递参数的方式取决于可调用对象的类型：

* **自由函数/成员函数：** 参数直接跟在函数名/成员函数指针后面传递给 `setTask`。内部通过 `std::bind` 完成绑定。对于成员函数，第一个参数必须是对象实例或对象指针。
* **Lambda 表达式：** 参数通过 lambda 的**捕获列表** (`[]`) 或**参数列表**传递。对于异步执行的任务，使用捕获列表通常更方便，捕获方式可以是值捕获 (`[=]`, `[var]`) 或引用捕获 (`[&]`, `[&var]`)。请注意，如果捕获局部变量的引用 (`[&]`, `[&var]`)，需要确保在任务执行期间，该局部变量的生命周期仍然有效，这在跨线程异步场景中需要特别小心。值捕获通常更安全。

```cpp
#include "LSX_LIB/Thread/ThreadWrapper.h"
#include "LSX_LIB/Thread/Scheduler.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <string>

// 自由函数带参数 (已在上面示例 2.1.3 演示)
void greet(const std::string& message, int times) {
    for (int i = 0; i < times; ++i) {
        std::cout << "Greeting: " << message << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

int main() {
    using namespace LSX_LIB::Thread;

    // ThreadWrapper 绑定自由函数带参数
    ThreadWrapper tw_greet;
    tw_greet.setTask(greet, "Hello LSX!", 3); // 参数直接跟在函数名后面
    tw_greet.start();
    tw_greet.stop();

    std::cout << std::endl;

    // Scheduler 安排带参数的 Lambda 任务
    Scheduler sched_params;
    int count_limit = 3;
    std::string task_name = "参数化任务";

    sched_params.scheduleOnce(500, [count_limit, task_name](){ // Lambda 值捕获局部变量
        std::cout << "Scheduled once: Task \"" << task_name << "\" with limit " << count_limit << std::endl;
    });

    sched_params.schedulePeriodic(1000, [=](){ // Lambda 值捕获所有可捕获变量
         static int current_count = 0;
         std::cout << "Scheduled periodic: Task \"" << task_name << "\" (limit " << count_limit << "), tick " << ++current_count << std::endl;
         if (current_count >= count_limit) {
             // 周期任务内部达到条件，通知外部停止调度器（在实际应用中需要更安全的通知机制）
             // 为了示例简单，这里不直接退出
         }
    });

    std::this_thread::sleep_for(std::chrono::seconds(4));
    sched_params.shutdown();

    return 0;
}
```

### 5. 获取任务结果与线程间通信

基础的 `ThreadWrapper` 执行任务后不会直接返回结果。如果您需要在任务完成后获取数据或进行线程间通信，可以考虑以下方式：

* **修改共享变量：** 任务执行时修改一个主线程或其他线程可访问的共享变量。这需要您**严格使用互斥锁 (`std::mutex`) 或原子操作 (`std::atomic`)** 来保证线程安全。
* **使用回调函数：** 在绑定任务时，将一个回调函数作为参数传递进去。任务执行完成后调用这个回调函数，并将结果传递给它。
* **利用 `ICommunicator` 接口 (扩展)：** 实现 `ICommunicator` 接口，提供线程间发送消息和注册消息处理器的能力。这是一种更结构化的线程间通信方式。
* **结合 `std::future` 和 `std::promise`：** 您可以在主线程创建一个 `std::promise`，将与之关联的 `std::future` 保存在主线程，并将 `promise` 的一部分（例如通过 `std::shared_ptr<std::promise>`) 传递给任务。任务完成后通过 `promise.set_value()` 设置结果，主线程通过 `future.get()` 获取结果（会阻塞直到结果可用）。或者使用 `std::packaged_task` 结合 `ThreadWrapper` 来实现。

这些高级用法超出了基础库的直接功能，但库的设计（例如 `ThreadWrapper` 可以绑定任何 `std::function`）允许您在任务函数中实现这些机制。

### 6. 注意事项

* **线程安全：** 如果您的任务函数访问共享数据，必须采取适当的同步机制（互斥锁、读写锁、条件变量、原子操作等）来防止数据竞争。LSX_LIB::Thread 库本身内部是线程安全的（对其自身状态和列表的操作），但这不保证您提供的任务函数的线程安全。
* **异常处理：** `ThreadWrapper::threadFunc` 内部对 `task_()` 的调用使用了 `try-catch` 块来捕获异常并打印错误信息，然后通常会停止线程。您的任务函数中抛出的未捕获异常会被这里处理。您也可以在自己的任务函数内部处理特定异常。
* **Joinable vs Detached：** 理解 `start(true)` (detached) 和 `start(false)` (joinable) 的区别至关重要。Joinable 线程在 `stop()` 或 `shutdown()` 时会被等待完成，而 Detached 线程则不会，其资源在完成后自动释放。在 `Scheduler` 中，一次性任务 (`scheduleOnce`) 默认使用 detached，周期性任务 (`schedulePeriodic`) 默认使用 joinable，这是基于常见用法的考虑。如果您需要在 `scheduleOnce` 后等待任务完成，请修改其实现为非 detached。
* **Scheduler 任务清理：** 对于 `scheduleOnce` 创建的分离任务，尽管 `ThreadWrapper` 对象在任务完成后会自动销毁（如果没有其他 `shared_ptr` 引用），但 `Scheduler` 内部 `tasks_` 列表中的 `shared_ptr` 需要显式移除。目前的 `shutdown()` 会清空列表，但在长时间运行且频繁调用 `scheduleOnce` 的应用中，考虑定期清理列表中已停止 (`ThreadState::STOPPED`) 的分离任务，以防止列表无限增长。周期性任务由于默认是 joinable 并在 `shutdown` 时被 join 和清理，通常问题不大。

---

通过本教程，您应该能够更灵活地使用 LSX_LIB::Thread 库来管理线程和调度任务，包括将不同类型的函数和方法作为任务在独立的线程中执行。