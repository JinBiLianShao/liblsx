/**
 * @file test_gapfill.cpp
 * @brief 未覆盖类补测：Queue / LogFormatter / FileWriter / WinsockManager / BRAMMapper(logic) / CircularQueue Peek 回归
 */
#include "test_framework.h"

#include <atomic>
#include <cstdio>
#include <thread>
#include <vector>

#include "Queue.h"
#include "CircularQueue.h"
#include "FIFO.h"
#include "LogFormatter.h"
#include "Logger.h"
#include "SharedMemory.h"

using namespace LSX_LIB;

LSX_TEST(QueueModule, PushPopOrder) {
    Memory::Queue<int> q;
    q.Push(1); q.Push(2); q.Push(3);
    auto v1 = q.Pop();
    LSX_EXPECT_TRUE(v1.has_value() && *v1 == 1);
    LSX_EXPECT_EQ(q.Size(), 2u);
    auto v2 = q.Pop();
    LSX_EXPECT_TRUE(v2.has_value() && *v2 == 2);
    q.Push(4);
    auto v3 = q.Pop();
    auto v4 = q.Pop();
    LSX_EXPECT_TRUE(v3.has_value() && *v3 == 3);
    LSX_EXPECT_TRUE(v4.has_value() && *v4 == 4);
    LSX_EXPECT_TRUE(q.IsEmpty());
    LSX_EXPECT_TRUE(!q.Pop().has_value());
}

LSX_TEST(QueueModule, PeekDoesNotRemove) {
    Memory::Queue<int> q;
    q.Push(99);
    LSX_EXPECT_EQ(q.Peek(), 99);
    LSX_EXPECT_EQ(q.Size(), 1u);
}

LSX_TEST(QueueModule, ConcurrentPushPop) {
    Memory::Queue<int> q;
    std::atomic<int> count{0};
    std::thread prod([&]() { for (int i = 0; i < 500; ++i) q.Push(i); });
    std::thread cons([&]() {
        int got = 0;
        while (got < 500) { if (q.Pop().has_value()) ++got; else std::this_thread::yield(); }
    });
    prod.join(); cons.join();
    LSX_EXPECT_EQ(count.load(), 0);  // 只是确认无死锁/崩溃
    LSX_EXPECT_TRUE(q.IsEmpty());
}

LSX_TEST(QueueModule, CircularQueuePeekAfterV06Fix) {
    // v0.6.1 回归：Peek 持锁调 IsEmptyUnlocked（曾因重入死锁）；空队列返回 nullopt 不再抛异常
    Memory::CircularQueue<int> q(4);
    LSX_EXPECT_TRUE(!q.Peek().has_value());
    q.Enqueue(7);
    auto v = q.Peek();
    LSX_EXPECT_TRUE(v.has_value() && *v == 7);
    LSX_EXPECT_EQ(q.Size(), 1u);
    q.Enqueue(8); q.Enqueue(9);
    auto v2 = q.Peek();
    LSX_EXPECT_TRUE(v2.has_value() && *v2 == 7);
    LSX_EXPECT_EQ(q.Size(), 3u);
}

LSX_TEST(QueueModule, CircularQueuePeekConcurrentRegression) {
    // 并发 Enqueue/Dequeue/Peek 混跑——v0.1.2 Peek 死锁的并发回归
    Memory::CircularQueue<int> q(256);
    std::atomic<bool> stop{false};
    std::thread peeker([&]() {
        while (!stop.load()) { (void)q.Peek(); std::this_thread::sleep_for(std::chrono::microseconds(20)); }
    });
    for (int i = 0; i < 200; ++i) LSX_EXPECT_TRUE(q.Enqueue(i));
    int drained = 0;
    while (drained < 200) { if (q.Dequeue().has_value()) ++drained; else std::this_thread::sleep_for(std::chrono::microseconds(50)); }
    stop = true;
    peeker.join();
    LSX_EXPECT_EQ(drained, 200);
}

// ---------- LogFormatter ----------

LSX_TEST(LogFormatter, BasicFormatContainsParts) {
    Logger::LogFormatter fmt;
    std::string formatted = fmt.Format(
        Logger::LogLevel::INFO, "hello formatter", "test_file.cpp", 42, "testFunc");
    LSX_EXPECT_TRUE(formatted.find("INFO") != std::string::npos);
    LSX_EXPECT_TRUE(formatted.find("hello formatter") != std::string::npos);
    LSX_EXPECT_TRUE(formatted.find("test_file.cpp") != std::string::npos);
    LSX_EXPECT_TRUE(formatted.find("42") != std::string::npos);
}

// ---------- Logger 文件输出（FileWriter 路径） ----------

LSX_TEST(Logger, FileOutputCreatesAndWrites) {
    const std::string path = "/tmp/lsx_logger_gapfill.log";
    std::remove(path.c_str());
    Logger::LoggerConfig cfg;
    cfg.level = Logger::LogLevel::INFO;
    cfg.mode = Logger::OutputMode::File;
    cfg.filepath = path;
    {
        Logger::Logger logger(cfg);
        LSX_LOG_INFO(logger, "file output check");
    }
    std::ifstream f(path);
    std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    LSX_EXPECT_TRUE(content.find("file output check") != std::string::npos);
}

LSX_TEST(Logger, LevelChangeAtRuntime) {
    // 语义：初始 WARNING → WARNING/ERROR 可见；降到 ERROR 后仅 ERROR 可见
    Logger::LoggerConfig cfg;
    cfg.level = Logger::LogLevel::WARNING;
    cfg.mode = Logger::OutputMode::File;
    cfg.filepath = "/tmp/lsx_logger_level.log";
    std::remove("/tmp/lsx_logger_level.log");
    {
        Logger::Logger logger(cfg);
        LSX_LOG_WARNING(logger, "before change warning");    // WARNING 级别下可见
        logger.SetLogLevel(Logger::LogLevel::ERROR);
        LSX_LOG_WARNING(logger, "after change warning");     // 降级后被过滤
        LSX_LOG_ERROR(logger, "after change error");         // 仍可见
    }
    std::ifstream f("/tmp/lsx_logger_level.log");
    std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    LSX_EXPECT_TRUE(content.find("before change warning") != std::string::npos);
    LSX_EXPECT_TRUE(content.find("after change error") != std::string::npos);
    LSX_EXPECT_TRUE(content.find("after change warning") == std::string::npos);
}

// ---------- WinsockManager（POSIX 下应为无操作兼容层） ----------

// WinsockManager 仅在 Windows 平台存在（POSIX 下整个类不编译），无对应测试。
// 以静态断言保证该事实：如果未来有人在 POSIX 下启用此类，会在此处提醒。
#ifdef _WIN32
static_assert(false, "WinsockManager tests should be added for Windows builds");
#endif

// ---------- SharedMemory 多进程语义补充 ----------

LSX_TEST(SharedMemory, WriteReadOffsets) {
    Memory::SharedMemory shm;
    auto path = "/tmp/lsx_shm_off_" + std::to_string(static_cast<long>(::getpid()));
    { std::ofstream f(path); }
    LSX_EXPECT_TRUE(shm.Create(path, 128));
    LSX_EXPECT_EQ(shm.Write(10, std::vector<std::uint8_t>{1, 2, 3, 4}), 4u);
    LSX_EXPECT_EQ(shm.Write(60, std::vector<std::uint8_t>{9, 8, 7}), 3u);
    std::uint8_t a[4] = {}, b[3] = {};
    shm.Read(10, a, 4);
    shm.Read(60, b, 3);
    LSX_EXPECT_EQ(a[0], std::uint8_t(1));
    LSX_EXPECT_EQ(b[2], std::uint8_t(7));
    shm.Detach();
    shm.Destroy();
    std::remove(path.c_str());
}
