/**
 * @file example_lock.cpp
 * @brief LIBLSX LockManager 模块使用示例
 *
 * 演示内容：
 *   1. LockGuard        —— 任意可加锁类型的 RAII 独占锁（比 std::lock_guard 更泛化）
 *   2. SharedLockGuard  —— 读写锁的共享（读）锁 RAII 封装
 *   3. MultiLockGuard   —— 原子锁定多把锁，内置死锁避免（std::scoped_lock 封装）
 *   4. Condition        —— 条件变量（强制谓词，规避虚假唤醒）
 *
 * 关键点：
 *   - 全部 RAII：作用域结束或抛异常时自动解锁，无需手写 unlock。
 *   - MultiLockGuard 用 std::scoped_lock 一次性拿多把锁，避免「锁顺序不同导致死锁」。
 *   - Condition 的 wait 带谓词，返回前自动重新检查条件，天然免疫虚假唤醒。
 */

#include "LockManager/LockGuard.h"
#include "LockManager/SharedLockGuard.h"
#include "LockManager/MultiLockGuard.h"
#include "LockManager/Condition.h"

#include <chrono>
#include <iostream>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <thread>
#include <vector>

namespace lm = LSX_LIB::LockManager;

// ---------------------------------------------------------------------------
// 演示 1：LockGuard —— RAII 独占锁
// ---------------------------------------------------------------------------
static void demo_lock_guard()
{
    std::cout << "\n===== 演示 1：LockGuard 独占锁 =====\n";

    std::mutex m;
    int counter = 0;

    auto worker = [&]() {
        for (int i = 0; i < 1000; ++i) {
            // 构造即加锁，离开花括号自动解锁；异常也安全
            lm::LockGuard<std::mutex> guard(m);
            ++counter;
        }
    };

    std::thread t1(worker), t2(worker);
    t1.join();
    t2.join();
    std::cout << "  两个线程各加 1000 次，结果 " << counter << "（应为 2000）\n";
}

// ---------------------------------------------------------------------------
// 演示 2：SharedLockGuard —— 读写锁的共享（读）锁
// ---------------------------------------------------------------------------
static void demo_shared_lock()
{
    std::cout << "\n===== 演示 2：SharedLockGuard 共享锁 =====\n";

    std::shared_mutex rw;
    int data = 42;

    // 写线程：独占锁
    auto writer = [&]() {
        lm::LockGuard<std::shared_mutex> w(rw);  // 独占写锁
        data = 100;
    };

    // 读线程：共享锁，多个读可并发
    auto reader = [&]() {
        lm::SharedLockGuard<std::shared_mutex> r(rw);  // 共享读锁
        std::cout << "  [reader] 读到 " << data << "\n";
    };

    std::thread w(writer), r1(reader), r2(reader);
    w.join();
    r1.join();
    r2.join();
}

// ---------------------------------------------------------------------------
// 演示 3：MultiLockGuard —— 原子锁定多把锁，避免死锁
// ---------------------------------------------------------------------------
static void demo_multi_lock()
{
    std::cout << "\n===== 演示 3：MultiLockGuard 多锁 =====\n";

    std::mutex a, b;
    int balance_a = 100, balance_b = 100;

    // 转账：两个线程故意按相反顺序拿 a/b 两把锁，
    // MultiLockGuard 用 std::scoped_lock 原子获取，不会因顺序不同而死锁。
    auto transfer = [&](std::mutex& first, std::mutex& second, int amount) {
        lm::MultiLockGuard<std::mutex, std::mutex> g(first, second);
        balance_a -= amount;
        balance_b += amount;
    };

    std::thread t1(transfer, std::ref(a), std::ref(b), 30);  // 先 a 后 b
    std::thread t2(transfer, std::ref(b), std::ref(a), 50);  // 先 b 后 a
    t1.join();
    t2.join();

    // 净额不变（+30-30-50+50），且没有死锁
    std::cout << "  转账后 a=" << balance_a << ", b=" << balance_b
              << "（总和应仍为 200）\n";
}

// ---------------------------------------------------------------------------
// 演示 4：Condition —— 条件变量（生产者-消费者）
// ---------------------------------------------------------------------------
static void demo_condition()
{
    std::cout << "\n===== 演示 4：Condition 条件变量 =====\n";

    std::mutex m;
    lm::Condition cv;
    bool ready = false;
    int value = 0;

    // 消费者：等待 ready 为真
    std::thread consumer([&]() {
        std::unique_lock<std::mutex> lock(m);
        // 带谓词的 wait：内部循环检查 pred，免疫虚假唤醒
        cv.wait(lock, [&] { return ready; });
        std::cout << "  [consumer] 收到 value = " << value << "\n";
    });

    // 生产者：写入数据后置 ready 并通知
    std::thread producer([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        {
            std::lock_guard<std::mutex> lock(m);
            value = 99;
            ready = true;
        }
        cv.notify_one();
    });

    producer.join();
    consumer.join();
}

int main()
{
    demo_lock_guard();
    demo_shared_lock();
    demo_multi_lock();
    demo_condition();
    return 0;
}
