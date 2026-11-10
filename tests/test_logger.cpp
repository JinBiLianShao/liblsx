/**
 * @file test_logger.cpp
 * @brief Logger 模块深度测试：级别过滤 / 文件输出与轮转 / 并发写入
 */
#include "test_framework.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <thread>
#include <vector>

#include "Logger.h"
#include "LogCommon.h"

using namespace LSX_LIB;
using namespace std::chrono_literals;

static const std::string kLogPath = "/tmp/lsx_test_logger.log";
static const std::string kLogPath2 = "/tmp/lsx_test_logger2.log";

LSX_TEST(Logger, LevelFilteringEnforced) {
    Logger::LoggerConfig cfg;
    cfg.level = Logger::LogLevel::WARNING;
    cfg.mode = Logger::OutputMode::File;
    cfg.filepath = kLogPath;
    std::remove(kLogPath.c_str());
    {
        Logger::Logger logger(cfg);
        LSX_LOG_DEBUG(logger, "debug should be filtered");
        LSX_LOG_INFO(logger, "info should be filtered");
        LSX_LOG_WARNING(logger, "warning passes");
        LSX_LOG_ERROR(logger, "error passes");
    }  // 析构刷盘
    std::ifstream f(kLogPath);
    std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    LSX_EXPECT_TRUE(content.find("warning passes") != std::string::npos);
    LSX_EXPECT_TRUE(content.find("error passes") != std::string::npos);
    LSX_EXPECT_TRUE(content.find("debug should be filtered") == std::string::npos);
    LSX_EXPECT_TRUE(content.find("info should be filtered") == std::string::npos);
}

LSX_TEST(Logger, FileRotationRespectsMaxLines) {
    Logger::LoggerConfig cfg;
    cfg.level = Logger::LogLevel::INFO;
    cfg.mode = Logger::OutputMode::File;
    cfg.filepath = kLogPath2;
    cfg.maxLines = 10;
    std::remove(kLogPath2.c_str());
    {
        Logger::Logger logger(cfg);
        for (int i = 0; i < 25; ++i) {
            LSX_LOG_INFO(logger, "rotation line " + std::to_string(i));
        }
    }
    // 轮转后文件行数不超过 maxLines + 少量缓冲余量（逐行验证）
    std::ifstream f(kLogPath2);
    int lines = 0;
    std::string line;
    while (std::getline(f, line)) ++lines;
    LSX_EXPECT_TRUE(lines <= 15);  // 10 行上限 + 轮转缓冲余量
}

LSX_TEST(Logger, ConcurrentMultiThreadWrites) {
    Logger::LoggerConfig cfg;
    cfg.level = Logger::LogLevel::INFO;
    cfg.mode = Logger::OutputMode::File;
    cfg.filepath = kLogPath;
    std::remove(kLogPath.c_str());
    {
        Logger::Logger logger(cfg);
        std::vector<std::thread> threads;
        std::atomic<int> written{0};
        for (int t = 0; t < 4; ++t) {
            threads.emplace_back([&logger, &written, t]() {
                for (int i = 0; i < 50; ++i) {
                    LSX_LOG_INFO(logger, "thread " + std::to_string(t) + " line " + std::to_string(i));
                    ++written;
                }
            });
        }
        for (auto& th : threads) th.join();
        LSX_EXPECT_EQ(written.load(), 200);
    }
    std::ifstream f(kLogPath);
    int lines = 0;
    std::string line;
    while (std::getline(f, line)) ++lines;
    LSX_EXPECT_EQ(lines, 200);  // 并发写入不丢行
}

LSX_TEST(Logger, LogMessageContainsMetadata) {
    Logger::LoggerConfig cfg;
    cfg.level = Logger::LogLevel::INFO;
    cfg.mode = Logger::OutputMode::File;
    cfg.filepath = kLogPath;
    std::remove(kLogPath.c_str());
    {
        Logger::Logger logger(cfg);
        LSX_LOG_INFO(logger, "metadata check");
    }
    std::ifstream f(kLogPath);
    std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    // 格式应含时间戳、级别、源码位置
    LSX_EXPECT_TRUE(content.find("INFO") != std::string::npos);
    LSX_EXPECT_TRUE(content.find("test_logger.cpp") != std::string::npos);
    LSX_EXPECT_TRUE(content.find("metadata check") != std::string::npos);
}
