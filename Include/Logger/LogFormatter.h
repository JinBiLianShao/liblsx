#ifndef LSX_LIB_LOGGER_LOG_FORMATTER_H
#define LSX_LIB_LOGGER_LOG_FORMATTER_H
#pragma once
#include "LogCommon.h"
#include <string>
#include <chrono>   // 用于时间戳
#include <sstream>  // 用于字符串拼接
#include <iomanip>  // 用于 std::put_time
#include <thread>   // 用于 std::this_thread::get_id

#ifdef _WIN32
#include <windows.h> // For GetCurrentThreadId() on Windows if needed, though std::this_thread::get_id() is preferred
#endif


namespace LSX_LIB {
    namespace Logger {

        class LogFormatter {
        public:
            LogFormatter() = default;

            /**
             * @brief 格式化日志消息.
             * @param level 日志级别.
             * @param msg 日志消息体.
             * @param file 源代码文件名.
             * @param line 源代码行号.
             * @param func 源代码函数名.
             * @return 格式化后的日志字符串.
             */
            std::string Format(LogLevel level,
                               const std::string& msg,
                               const char* file,
                               int line,
                               const char* func);

        private:
            std::string LogLevelToString(LogLevel level);
        };

    } // namespace Logger
} // namespace LSX_LIB

#endif // LSX_LIB_LOGGER_LOG_FORMATTER_H