/**
 * @file test_lock.cpp
 * @brief LockManager 模块深度测试：各锁类型 RAII / MultiLockGuard / Condition
 */
#include "test_framework.h"

#include <atomic>
#include <chrono>
#include <shared_mutex>
#include <thread>
#include <vector>

#include "LockGuard.h"
#include "SharedLockGuard.h"
#include "MultiLockGuard.h"
#include "Condition.h"

using namespace LSX_LIB;
using namespace std::chrono_literals;

LSX_TEST(LockGuard, MutexExclusiveAccess) {
    std::mutex m;
    int counter = 0;
    std::vector<std::thread> threads;
    for (int t = 0; t < 8; ++t) {
        threads.emplace_back([&]() {
            for (int i = 0; i < 500; ++i) {
                LockManager::LockGuard<std::mutex> guard(m);
                ++counter;
            }
        });
    }
    for (auto& th : threads) th.join();
    LSX_EXPECT_EQ(counter, 4000);
}

LSX_TEST(LockGuard, RecursiveMutexAllowsReentry) {
    std::recursive_mutex m;
    int depth_ok = 0;
    {
        LockManager::LockGuard<std::recursive_mutex> g1(m);
        {
            LockManager::LockGuard<std::recursive_mutex> g2(m);
            depth_ok = 1;
        }
    }
    LSX_EXPECT_EQ(depth_ok, 1);
}

LSX_TEST(LockGuard, TimedMutexHoldAndRelease) {
    std::timed_mutex m;
    std::atomic<bool> got_first{false}, acquired_after{false};
    std::thread holder([&]() {
        LockManager::LockGuard<std::timed_mutex> g(m);
        got_first = true;
        std::this_thread::sleep_for(100ms);
    });
    holder.join();
    // 持有者退出后应能立即再获取
    {
        LockManager::LockGuard<std::timed_mutex> g(m);
        acquired_after = true;
    }
    LSX_EXPECT_TRUE(got_first.load());
    LSX_EXPECT_TRUE(acquired_after.load());
}

LSX_TEST(SharedLockGuard, TimedTryLockFailsWhileExclusiveHeld) {
    std::shared_timed_mutex m;
    std::unique_lock<std::shared_timed_mutex> exclusive(m);  // 真正的独占持有（写锁）
    LockManager::SharedLockGuard<std::shared_timed_mutex> contender(m, std::defer_lock);
    bool got = contender.try_lock_shared_for(std::chrono::milliseconds(50));
    LSX_EXPECT_TRUE(!got);  // 独占持有期间共享获取应失败
    exclusive.unlock();
    bool got2 = contender.try_lock_shared_for(std::chrono::milliseconds(50));
    LSX_EXPECT_TRUE(got2);  // 释放后共享获取应成功
}

LSX_TEST(SharedLockGuard, ReadersParallelWriterExclusive) {
    std::shared_mutex m;
    std::atomic<int> concurrent_readers{0}, max_readers{0};
    std::vector<std::thread> readers;
    for (int t = 0; t < 4; ++t) {
        readers.emplace_back([&]() {
            for (int i = 0; i < 50; ++i) {
                LockManager::SharedLockGuard<std::shared_mutex> g(m);  // shared/read
                int now = ++concurrent_readers;
                int prev = max_readers.load();
                while (now > prev && !max_readers.compare_exchange_weak(prev, now)) {}
                std::this_thread::sleep_for(2ms);
                --concurrent_readers;
            }
        });
    }
    for (auto& th : readers) th.join();
    LSX_EXPECT_TRUE(max_readers.load() >= 2);  // 读锁应允许并行
}

LSX_TEST(MultiLockGuard, DeadlockAvoidanceDualOrder) {
    // 经典 AB/BA 死锁场景：两个线程以相反顺序拿锁，scoped_lock 语义应保证都不死锁
    std::mutex m1, m2;
    std::atomic<int> done{0};
    std::thread t1([&]() {
        for (int i = 0; i < 100; ++i) {
            LockManager::MultiLockGuard<std::mutex, std::mutex> g(m1, m2);
            ++done;
        }
    });
    std::thread t2([&]() {
        for (int i = 0; i < 100; ++i) {
            LockManager::MultiLockGuard<std::mutex, std::mutex> g(m2, m1);  // 反序
            ++done;
        }
    });
    t1.join();
    t2.join();
    LSX_EXPECT_EQ(done.load(), 200);  // 能数完即未死锁
}

LSX_TEST(MultiLockGuard, TripleLockGuard) {
    std::mutex a, b, c;
    {
        LockManager::MultiLockGuard<std::mutex, std::mutex, std::mutex> g(a, b, c);
    }
    LSX_EXPECT_TRUE(true);  // 能构造能析构即通过
}

LSX_TEST(Condition, PredicateWaitAvoidsSpuriousWakeup) {
    // Condition 封装 std::condition_variable_any，要求 BasicLockable —— 用 std::unique_lock
    std::mutex m;
    bool ready = false;
    LockManager::Condition cond;
    std::atomic<bool> predicate_seen{false};
    std::thread waiter([&]() {
        std::unique_lock<std::mutex> lock(m);
        cond.wait(lock, [&]() { return ready; });  // 强制谓词
        predicate_seen = true;
    });
    std::this_thread::sleep_for(50ms);
    LSX_EXPECT_TRUE(!predicate_seen.load());  // 谓词未满足前不醒
    {
        std::lock_guard<std::mutex> lock(m);
        ready = true;
    }
    cond.notify_one();
    waiter.join();
    LSX_EXPECT_TRUE(predicate_seen.load());
}

LSX_TEST(Condition, NotifyAllWakesEveryone) {
    std::mutex m;
    bool go = false;
    LockManager::Condition cond;
    std::atomic<int> finished{0};
    std::vector<std::thread> waiters;
    for (int i = 0; i < 4; ++i) {
        waiters.emplace_back([&]() {
            std::unique_lock<std::mutex> lock(m);
            cond.wait(lock, [&]() { return go; });
            ++finished;
        });
    }
    std::this_thread::sleep_for(50ms);
    {
        std::lock_guard<std::mutex> lock(m);
        go = true;
    }
    cond.notify_all();
    for (auto& th : waiters) th.join();
    LSX_EXPECT_EQ(finished.load(), 4);
}
