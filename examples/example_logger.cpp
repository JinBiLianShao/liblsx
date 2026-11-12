/**
 * @file example_logger.cpp
 * @brief LIBLSX Logger 模块使用示例
 *
 * 演示内容：
 *   1. 级别过滤（DEBUG/INFO/WARNING/ERROR）
 *   2. 控制台输出 与 文件输出切换
 *   3. 文件日志按最大行数轮转
 *   4. 日志宏的便捷用法
 *
 * 关键点：
 *   - 日志级别低于当前级别会被过滤掉，动态可调（SetLogLevel）。
 *   - 文件模式需提供 filepath；maxLines 控制文件最多保留的行数。
 *   - 宏 LSX_LOG_DEBUG/INFO/WARNING/ERROR 自动带上文件名、行号、函数名。
 */

#include "Logger/Logger.h"

#include <iostream>
#include <string>

namespace lg = LSX_LIB::Logger;

int main()
{
    std::cout << "===== Logger 日志系统示例 =====\n";

    // ---- 1) 控制台输出，级别 DEBUG ----
    lg::LoggerConfig cfg;
    cfg.level = lg::LogLevel::DEBUG;
    cfg.mode = lg::OutputMode::Console;
    lg::Logger logger(cfg);

    // 宏形式：自动记录代码位置（文件:行号 函数名）
    LSX_LOG_DEBUG(logger, "这是一条调试信息");
    LSX_LOG_INFO(logger, "应用程序启动");
    LSX_LOG_WARNING(logger, "发现潜在问题");
    LSX_LOG_ERROR(logger, "发生严重错误");

    // ---- 2) 动态调级别：DEBUG 被过滤 ----
    logger.SetLogLevel(lg::LogLevel::WARNING);
    LSX_LOG_DEBUG(logger, "这条 DEBUG 不会输出");
    LSX_LOG_INFO(logger, "这条 INFO 也不会输出");
    LSX_LOG_WARNING(logger, "这条 WARNING 会输出");

    // ---- 3) 切到文件输出，最大 3 行轮转 ----
    logger.SetLogLevel(lg::LogLevel::DEBUG);
    cfg.filepath = "/tmp/lsx_log_example.log";
    cfg.maxLines = 3;
    lg::Logger file_logger(cfg);      // 用带文件路径的配置重建
    file_logger.SetOutputMode(lg::OutputMode::File);

    for (int i = 1; i <= 5; ++i) {
        LSX_LOG_INFO(file_logger, "第 " + std::to_string(i) + " 条日志");
    }
    // 文件里只保留最近 3 条（第 3/4/5 条）
    std::cout << "  文件日志已写入 /tmp/lsx_log_example.log（只保留最近 3 条）\n";

    file_logger.Flush();              // 确保落盘（析构也会 flush）
    return 0;
}
