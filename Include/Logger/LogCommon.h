#ifndef LSX_LIB_LOGGER_LOG_COMMON_H
#define LSX_LIB_LOGGER_LOG_COMMON_H
#pragma once
#include <string>

namespace LSX_LIB {
    namespace Logger {

        // 日志级别枚举
        enum class LogLevel {
            DEBUG = 0,
            INFO,
            WARNING,
            ERROR
        };

        // 输出目标模式枚举
        enum class OutputMode {
            Console,
            File
        };

        // 日志配置结构体
        struct LoggerConfig {
            LogLevel level = LogLevel::INFO;         // 默认日志级别
            OutputMode mode = OutputMode::Console;   // 默认输出模式
            std::string filepath = "app.log";      // 默认日志文件路径
            int maxLines = 1000;                   // 默认文件最大行数 (对于文件输出)

            LoggerConfig() = default;

            LoggerConfig(LogLevel lvl, OutputMode m, std::string path, int lines)
                : level(lvl), mode(m), filepath(std::move(path)), maxLines(lines) {}
        };

    } // namespace Logger
} // namespace LSX_LIB

#endif // LSX_LIB_LOGGER_LOG_COMMON_H