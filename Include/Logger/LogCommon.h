/**
 * @file Log_Common.h
 * @brief 数据传输工具库 - 日志模块通用定义
 * @details 定义了 LSX_LIB::Logger 命名空间下的日志模块通用枚举和结构体，
 * 包括日志级别 (LogLevel)、输出目标模式 (OutputMode) 和日志配置结构体 (LoggerConfig)。
 * 这些定义为日志系统的不同组件提供了基础类型和配置信息。
 * @author 连思鑫（liansixin）
 * @date 2025年5月13日
 * @version 1.0
 *
 * ### 核心功能
 * - **日志级别**: 定义不同严重程度的日志消息级别。
 * - **输出模式**: 定义日志消息的输出目标（控制台、文件等）。
 * - **日志配置**: 结构体用于存储日志系统的配置参数，如级别、模式、文件路径和文件大小限制。
 *
 * ### 使用示例
 *
 * @code
 * #include "Log_Common.h"
 * #include <iostream>
 *
 * int main() {
 * // 使用 LogLevel 枚举
 * LSX_LIB::Logger::LogLevel currentLevel = LSX_LIB::Logger::LogLevel::WARNING;
 * std::cout << "Current log level (enum value): " << static_cast<int>(currentLevel) << std::endl;
 *
 * // 使用 OutputMode 枚举
 * LSX_LIB::Logger::OutputMode currentMode = LSX_LIB::Logger::OutputMode::File;
 * // 可以根据模式进行判断或切换输出逻辑
 * if (currentMode == LSX_LIB::Logger::OutputMode::File) {
 * std::cout << "Output mode is File." << std::endl;
 * }
 *
 * // 使用 LoggerConfig 结构体
 * LSX_LIB::Logger::LoggerConfig config; // 使用默认构造函数
 * std::cout << "Default log level: " << static_cast<int>(config.level) << std::endl;
 * std::cout << "Default output mode: " << static_cast<int>(config.mode) << std::endl;
 * std::cout << "Default file path: " << config.filepath << std::endl;
 * std::cout << "Default max lines: " << config.maxLines << std::endl;
 *
 * // 使用带参数的构造函数
 * LSX_LIB::Logger::LoggerConfig customConfig(
 * LSX_LIB::Logger::LogLevel::DEBUG,
 * LSX_LIB::Logger::OutputMode::Console,
 * "debug.log",
 * 500
 * );
 * std::cout << "Custom log level: " << static_cast<int>(customConfig.level) << std::endl;
 * std::cout << "Custom output mode: " << static_cast<int>(customConfig.mode) << std::endl;
 * std::cout << "Custom file path: " << customConfig.filepath << std::endl;
 * std::cout << "Custom max lines: " << customConfig.maxLines << std::endl;
 *
 * return 0;
 * }
 * @endcode
 *
 * ### 注意事项
 * - 这些定义是日志系统的基础类型，不包含日志记录的具体实现逻辑。
 * - LoggerConfig 结构体是按值传递的，修改拷贝不会影响原始配置。
 */

#ifndef LSX_LIB_LOGGER_LOG_COMMON_H
#define LSX_LIB_LOGGER_LOG_COMMON_H
#pragma once
#include <string> // 包含 std::string
#include <utility> // 包含 std::move

/**
 * @brief LSX 库的根命名空间。
 */
namespace LSX_LIB {
    /**
     * @brief 日志相关的命名空间。
     * 包含日志系统相关的类和工具。
     */
    namespace Logger {

        /**
         * @brief 日志级别枚举。
         * 定义了日志消息的不同严重程度。
         */
        enum class LogLevel {
            /**
             * @brief 调试级别。
             * 最详细的日志信息，用于开发和调试。
             */
            DEBUG = 0,
            /**
             * @brief 信息级别。
             * 一般性的信息消息，指示应用程序的进展。
             */
            INFO,
            /**
             * @brief 警告级别。
             * 可能指示潜在问题或非关键错误。
             */
            WARNING,
            /**
             * @brief 错误级别。
             * 指示发生了错误，可能影响应用程序的功能。
             */
            ERROR
        };

        /**
         * @brief 输出目标模式枚举。
         * 定义了日志消息的输出目标。
         */
        enum class OutputMode {
            /**
             * @brief 控制台输出。
             * 日志消息输出到标准控制台 (stdout/stderr)。
             */
            Console,
            /**
             * @brief 文件输出。
             * 日志消息写入到指定的文件。
             */
            File
        };

        /**
         * @brief 日志配置结构体。
         * 存储日志系统的配置参数。
         */
        struct LoggerConfig {
            /**
             * @brief 默认日志级别。
             * 只有大于或等于此级别的日志消息才会被处理和输出。
             */
            LogLevel level = LogLevel::INFO;         // 默认日志级别
            /**
             * @brief 默认输出模式。
             * 指定日志消息的输出目标。
             */
            OutputMode mode = OutputMode::Console;   // 默认输出模式
            /**
             * @brief 默认日志文件路径。
             * 当 OutputMode 为 File 时，指定日志文件的路径。
             */
            std::string filepath = "app.log";      // 默认日志文件路径
            /**
             * @brief 默认文件最大行数。
             * 当 OutputMode 为 File 时，指定单个日志文件的最大行数，用于文件滚动或截断。
             */
            int maxLines = 1000;                   // 默认文件最大行数 (对于文件输出)

            /**
             * @brief 默认构造函数。
             * 使用默认值初始化 LoggerConfig 结构体。
             */
            LoggerConfig() = default;

            /**
             * @brief 带参数的构造函数。
             * 使用指定的值初始化 LoggerConfig 结构体。
             *
             * @param lvl 日志级别。
             * @param m 输出模式。
             * @param path 日志文件路径。
             * @param lines 文件最大行数。
             */
            LoggerConfig(LogLevel lvl, OutputMode m, std::string path, int lines)
                    : level(lvl), mode(m), filepath(std::move(path)), maxLines(lines) {}
        };

    } // namespace Logger
} // namespace LSX_LIB

#endif // LSX_LIB_LOGGER_LOG_COMMON_H
