/**
 * @file LogFormatter.h
 * @brief 数据传输工具库 - 日志模块格式化类
 * @details 定义了 LSX_LIB::Logger 命名空间下的 LogFormatter 类，
 * 用于格式化日志消息，将日志级别、时间戳、线程ID、源代码位置和消息体
 * 组合成一个统一的字符串格式，便于输出到控制台或文件。
 * @author 连思鑫（liansixin）
 * @date 2025年5月13日
 * @version 1.0
 *
 * ### 核心功能
 * - **日志格式化**: 将日志消息的各个组成部分格式化为字符串。
 * - **包含信息**: 格式化后的字符串通常包含日志级别、时间、线程ID、文件/行号/函数名和消息内容。
 * - **辅助函数**: 包含将 LogLevel 枚举转换为字符串的辅助方法。
 *
 * ### 使用示例
 *
 * @code
 * #include "LogFormatter.h"
 * #include <iostream>
 *
 * // 示例函数，用于演示在函数内记录日志
 * void example_function() {
 * LSX_LIB::Logger::LogFormatter formatter;
 * std::string formatted_log = formatter.Format(
 * LSX_LIB::Logger::LogLevel::DEBUG,
 * "This is a debug message.",
 * __FILE__,
 * __LINE__,
 * __func__
 * );
 * std::cout << formatted_log << std::endl;
 * }
 *
 * int main() {
 * LSX_LIB::Logger::LogFormatter formatter;
 *
 * // 格式化不同级别的日志消息
 * std::string info_log = formatter.Format(
 * LSX_LIB::Logger::LogLevel::INFO,
 * "Application started.",
 * __FILE__,
 * __LINE__,
 * __func__
 * );
 * std::cout << info_log << std::endl;
 *
 * std::string warning_log = formatter.Format(
 * LSX_LIB::Logger::LogLevel::WARNING,
 * "Configuration file not found.",
 * __FILE__,
 * __LINE__,
 * __func__
 * );
 * std::cout << warning_log << std::endl;
 *
 * example_function(); // 调用示例函数
 *
 * std::string error_log = formatter.Format(
 * LSX_LIB::Logger::LogLevel::ERROR,
 * "Failed to connect to database.",
 * __FILE__,
 * __LINE__,
 * __func__
 * );
 * std::cout << error_log << std::endl;
 *
 * return 0;
 * }
 * @endcode
 *
 * ### 注意事项
 * - LogFormatter 类本身不执行日志的输出，只负责生成格式化后的字符串。
 * - 格式化字符串的具体样式由 `Format` 方法的实现决定。
 * - `__FILE__`, `__LINE__`, `__func__` 是编译器提供的宏，用于获取源代码位置信息。
 * - 获取线程ID (`std::this_thread::get_id()`) 和当前时间 (`std::chrono`) 是 Format 方法内部需要实现的逻辑。
 */

#ifndef LSX_LIB_LOGGER_LOG_FORMATTER_H
#define LSX_LIB_LOGGER_LOG_FORMATTER_H
#pragma once
#include "LogCommon.h" // 包含日志通用定义 (LogLevel, LoggerConfig等)
#include <string> // 包含 std::string
#include <chrono>   // 用于时间戳 (std::chrono::system_clock, std::chrono::duration_cast)
#include <sstream>  // 用于字符串拼接 (std::stringstream)
#include <iomanip>  // 用于 std::put_time (时间格式化)
#include <thread>   // 用于 std::this_thread::get_id (获取线程ID)

#ifdef _WIN32
// #include <windows.h> // For GetCurrentThreadId() on Windows if needed, though std::this_thread::get_id() is preferred
#endif


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
         * @brief 日志格式化类。
         * 负责将日志消息的各个组成部分（级别、时间、位置、消息体等）
         * 格式化成一个统一的字符串。
         */
        class LogFormatter {
        public:
            /**
             * @brief 默认构造函数。
             * 初始化 LogFormatter 对象。
             */
            LogFormatter() = default;

            /**
             * @brief 格式化日志消息。
             * 将日志消息的各个组成部分（级别、消息体、源代码文件/行号/函数名）
             * 与当前时间戳和线程ID结合，格式化成一个字符串。
             *
             * @param level 日志级别。
             * @param msg 日志消息体。
             * @param file 源代码文件名 (通常使用 __FILE__ 宏)。
             * @param line 源代码行号 (通常使用 __LINE__ 宏)。
             * @param func 源代码函数名 (通常使用 __func__ 宏)。
             * @return 格式化后的日志字符串。
             */
            std::string Format(LogLevel level,
                               const std::string& msg,
                               const char* file,
                               int line,
                               const char* func);

        private:
            /**
             * @brief 将 LogLevel 枚举值转换为对应的字符串表示。
             *
             * @param level 日志级别枚举值。
             * @return 对应的日志级别字符串 (例如 "DEBUG", "INFO")。
             */
            std::string LogLevelToString(LogLevel level);
        };

    } // namespace Logger
} // namespace LSX_LIB

#endif // LSX_LIB_LOGGER_LOG_FORMATTER_H
