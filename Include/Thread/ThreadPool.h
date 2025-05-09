/************************************************************
 * @file ThreadPool.h
 * @brief 实现一个线程池，用于管理多个工作线程并执行任务队列中的任务。
 * 该类继承自 IThreadPool 接口。
 *
 * 创建于 2024-03-07 更新于2024/10/28
 * 作者：连思鑫
 *
 * @interface
 * - ThreadPool(size_t numThreads = std::thread::hardware_concurrency()) : stop(false): 构造函数，创建指定数量的工作线程
 * - void enqueue(std::function<void()> task) override: 实现 IThreadPool 接口，将无返回值无参数的任务添加到任务队列中
 * - template<class F, class... Args>
 * auto enqueue(F&& f, Args&&... args) -> std::future<typename std::result_of<F(Args...)>::type>: 将任意可调用对象作为任务添加到任务队列中，并返回对应的 std::future 对象
 * - void shutdown() override: 实现 IThreadPool 接口，关闭线程池，并等待所有工作线程退出
 * - ~ThreadPool(): 析构函数，调用 shutdown() 方法
 *
 * @example
 * @code
 * #include <iostream>
 * #include <unistd.h>
 * #include <ThreadPool.h>
 * #include <IThreadPool.h> // 包含接口头文件
 * #include <future>
 *
 * // 示例任务函数 (无返回值无参数，符合 IThreadPool::enqueue 签名)
 * void simpleTask() {
 * pthread_t tid = pthread_self();
 * std::cout << "Simple Task started with ID: " << tid << std::endl;
 * std::cout << "Simple Task " << tid << ": Running" << std::endl;
 * // 模拟工作
 * std::this_thread::sleep_for(std::chrono::milliseconds(100));
 * std::cout << "Simple Task " << tid << ": Finished" << std::endl;
 * }
 *
 * // 示例任务函数 (带参数和返回值)
 * int complexTask(int x, int y) {
 * pthread_t tid = pthread_self();
 * std::cout << "Complex Task started with ID: " << tid << std::endl;
 * std::cout << "Complex Task " << tid << ": Running with " << x << ", " << y << std::endl;
 * // 模拟工作
 * std::this_thread::sleep_for(std::chrono::milliseconds(200));
 * int result = x + y;
 * std::cout << "Complex Task " << tid << ": Finished with result " << result << std::endl;
 * return result;
 * }
 *
 * int main() {
 * // 使用具体实现类
 * LSX_LIB::Thread::ThreadPool pool(2); // 创建一个包含 2 个线程的线程池
 *
 * // 通过 IThreadPool 接口提交任务 (无返回值无参数)
 * LSX_LIB::Thread::IThreadPool* i_pool = &pool;
 * i_pool->enqueue(simpleTask);
 * i_pool->enqueue(simpleTask);
 *
 * // 通过 ThreadPool 具体类提交任务 (带返回值和参数)
 * auto future1 = pool.enqueue(complexTask, 10, 20);
 * auto future2 = pool.enqueue([](const std::string& msg){
 * std::cout << "Lambda Task: " << msg << std::endl;
 * return msg.length();
 * }, "Hello Lambda!");
 *
 * // 获取 future 结果
 * std::cout << "Result of complexTask: " << future1.get() << std::endl;
 * std::cout << "Result of Lambda Task: " << future2.get() << std::endl;
 *
 * // 线程池在 main 函数结束时自动调用析构函数，进而调用 shutdown
 * // 或者可以显式调用 shutdown
 * // pool.shutdown();
 *
 * std::cout << "主线程退出。" << std::endl;
 * return 0;
 * }
 * @endcode
 ************************************************************/

#ifndef KKTRAFFIC_THREADPOOL_H
#define KKTRAFFIC_THREADPOOL_H
#pragma once
// 包含全局错误锁头文件
#include "GlobalErrorMutex.h"
// 包含 LIBLSX::LockManager::LockGuard 头文件
#include "LockGuard.h"
#include <iostream>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <vector>
#include <atomic>
#include <future>
#include <utility> // For std::forward, std::move
#include <type_traits> // For std::result_of (C++11/14), std::invoke_result (C++17+)

// 包含 IThreadPool 接口定义
#include "IThreadPool.h"

/**
 * @brief LSX 库的根命名空间。
 */
namespace LSX_LIB
{
    /**
     * @brief 线程相关的命名空间。
     * 包含线程管理和线程间通信相关的类和工具。
     */
    namespace Thread
    {
        /**
         * @brief 线程池类，管理多个工作线程并执行任务队列中的任务。
         * 该类继承自 IThreadPool 接口，提供了标准的线程池接口，
         * 并额外支持提交带返回值和参数的任务。
         */
        class ThreadPool : public Thread::IThreadPool // 继承 IThreadPool 接口
        {
        public:
            /**
             * @brief 构造函数，创建指定数量的工作线程。
             *
             * 初始化线程池，创建指定数量的工作线程并使其进入等待状态，准备执行任务。
             *
             * @param numThreads 线程池中工作线程的数量，默认为硬件并发线程数。
             */
            ThreadPool(size_t numThreads = std::thread::hardware_concurrency()) : stop(false)
            {
                // 确保线程数量至少为 1
                if (numThreads == 0) numThreads = 1;

                // 创建工作线程
                for (size_t i = 0; i < numThreads; ++i)
                {
                    workers.emplace_back([this]
                    {
                        // 每个工作线程的主循环
                        while (true)
                        {
                            std::function<void()> task;
                            {
                                // 锁定任务队列，等待任务或停止信号
                                std::unique_lock<std::mutex> lock(queue_mutex);

                                // 等待条件：线程池停止或任务队列不为空
                                condition.wait(lock, [this] { return stop || !tasks.empty(); });

                                // 如果线程池停止且任务队列为空，则线程退出
                                if (stop && tasks.empty())
                                    return;

                                // 从任务队列中取出任务
                                task = std::move(tasks.front());
                                tasks.pop();
                            }

                            // 执行任务
                            task();
                        }
                    });
                }
            }

            /**
             * @brief 实现 IThreadPool 接口，向任务队列添加一个无返回值无参数的任务。
             *
             * 将一个 `std::function<void()>` 类型的任务添加到任务队列中。
             *
             * @param task 要执行的无返回值无参数任务。
             */
            void enqueue(std::function<void()> task) override // 实现 IThreadPool::enqueue
            {
                {
                    // 锁定任务队列，添加任务
                    std::lock_guard<std::mutex> lock(queue_mutex);

                    // 检查线程池是否已停止
                    if (stop)
                    {
                        // 根据需求，可以抛出异常或记录错误
                        std::cerr << "ThreadPool is stopped, cannot enqueue task." << std::endl;
                        return; // 或者 throw std::runtime_error("enqueue on stopped ThreadPool");
                    }

                    // 将任务添加到队列
                    tasks.emplace(std::move(task));
                }
                // 通知一个等待中的工作线程有新任务
                condition.notify_one();
            }


            /**
             * @brief 向任务队列添加任意可调用对象作为任务，并返回对应的 future 对象。
             *
             * 将一个可调用对象（函数、lambda、函数对象等）及其参数包装成一个任务，
             * 添加到任务队列中，并返回一个 `std::future` 对象，用于获取任务的执行结果。
             *
             * @tparam F 任务函数类型。
             * @tparam Args 任务函数参数类型。
             * @param f 要执行的任务函数。
             * @param args 任务函数的参数。
             * @return std::future<typename std::result_of<F(Args...)>::type> 返回任务的 future 对象，用于获取任务执行结果。
             */
            template <class F, class... Args>
            auto enqueue(F&& f, Args&&... args) -> std::future<
#if __cplusplus >= 201703L
                std::invoke_result_t<F, Args...> // C++17 and later
#else
                typename std::result_of<F(Args...)>::type // C++11/14
#endif
            >
            {
                // 推导任务的返回类型
                using return_type =
#if __cplusplus >= 201703L
                    std::invoke_result_t<F, Args...>; // C++17 and later
#else
                        typename std::result_of<F(Args...)>::type; // C++11/14
#endif


                // 将可调用对象及其参数绑定，并包装到 std::packaged_task 中
                auto task = std::make_shared<std::packaged_task<return_type()>>(
                    std::bind(std::forward<F>(f), std::forward<Args>(args)...)
                );

                // 获取与 packaged_task 关联的 future
                std::future<return_type> res = task->get_future();
                {
                    // 锁定任务队列，添加包装后的任务
                    std::lock_guard<std::mutex> lock(queue_mutex);

                    // 检查线程池是否已停止
                    if (stop)
                    {
                        std::cerr << "ThreadPool is stopped, cannot enqueue task." << std::endl;
                        // 根据需求，可以抛出异常
                        // throw std::runtime_error("enqueue on stopped ThreadPool");
                        // 返回一个无效的 future
                        return std::future<return_type>();
                    }

                    // 将 packaged_task 的执行包装成 std::function<void()> 并添加到任务队列
                    tasks.emplace([task]() { (*task)(); });
                }
                // 通知一个等待中的工作线程有新任务
                condition.notify_one();
                // 返回 future 对象
                return res;
            }

            /**
             * @brief 实现 IThreadPool 接口，关闭线程池。
             *
             * 启动线程池的关闭过程。设置停止标志，通知所有工作线程退出，并等待它们完成当前任务后安全退出。
             * 此方法是阻塞的，直到所有工作线程都已退出。
             */
            void shutdown() override // 实现 IThreadPool::shutdown
            {
                {
                    // 锁定任务队列，设置停止标志
                    std::lock_guard<std::mutex> lock(queue_mutex);
                    stop = true;
                }
                // 通知所有等待中的工作线程
                condition.notify_all();
                // 等待所有工作线程退出
                for (std::thread& worker : workers)
                    if (worker.joinable()) // 检查线程是否可 join
                        worker.join();
            }

            /**
             * @brief 析构函数，关闭线程池。
             *
             * 在对象销毁时自动调用 shutdown() 方法，确保所有工作线程被停止并 join，释放资源。
             */
            ~ThreadPool()
            {
                shutdown(); // 调用 shutdown 方法
            }

        private:
            std::vector<std::thread> workers; /**< 工作线程数组。存储线程池中的工作线程对象。 */
            std::queue<std::function<void()>> tasks; /**< 任务队列。存储待执行的任务，每个任务是一个 `std::function<void()>`。 */
            std::mutex queue_mutex; /**< 互斥锁，保护任务队列的并发访问。 */
            std::condition_variable condition; /**< 条件变量，用于工作线程在任务队列为空时等待，以及在有新任务或线程池停止时被唤醒。 */
            std::atomic<bool> stop; /**< 原子布尔标志，指示线程池是否已请求停止。 */
        };
    }
} // namespace LSX_LIB

#endif // KKTRAFFIC_THREADPOOL_H
