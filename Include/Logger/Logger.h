#ifndef LSX_LIB_LOGGER_LOGGER_H
#define LSX_LIB_LOGGER_LOGGER_H
#pragma once
#include "LogCommon.h"
#include "LogFormatter.h"
#include "LogWriter.h"
#include <string>
#include <memory>
#include <mutex>
#include <atomic>

namespace LSX_LIB {
namespace Logger {

class Logger {
public:
    /**
     * @brief 构造函数.
     * @param config 日志配置.
     */
    explicit Logger(const LoggerConfig& config);

    ~Logger() = default;

    // 禁止拷贝和赋值
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    Logger(Logger&&) = delete;
    Logger& operator=(Logger&&) = delete;

    /**
     * @brief 记录日志.
     * @param msg_level 此条消息的日志级别.
     * @param msg 日志消息体.
     * @param file 源代码文件名 (通常使用 __FILE__).
     * @param line 源代码行号 (通常使用 __LINE__).
     * @param func 源代码函数名 (通常使用 __func__ 或 __FUNCTION__).
     */
    void Log(LogLevel msg_level,
             const std::string& msg,
             const char* file,
             int line,
             const char* func);

    /**
     * @brief 动态设置日志输出模式.
     * @param mode 新的输出模式 (Console 或 File).
     */
    void SetOutputMode(OutputMode mode);

    /**
     * @brief 获取当前输出模式.
     * @return 当前的输出模式.
     */
    OutputMode GetOutputMode() const;

    /**
     * @brief 动态设置日志记录级别.
     * @param new_level 新的日志级别. 低于此级别的日志将被忽略.
     */
    void SetLogLevel(LogLevel new_level);

    /**
     * @brief 获取当前日志记录级别.
     * @return 当前的日志级别.
     */
    LogLevel GetLogLevel() const;

    /**
     * @brief 手动刷新日志（主要用于文件输出）.
     */
    void Flush();


private:
    std::atomic<LogLevel> current_log_level_; // 当前日志记录级别
    LogFormatter formatter_;                  // 日志格式化器
    std::unique_ptr<LogWriter> writer_;       // 当前日志输出器
    mutable std::mutex writer_mutex_;         //保护 writer_ 的创建/切换以及写入操作

    // 从初始配置中保存的文件相关设置，用于在切换到文件模式时使用
    std::string config_filepath_;
    int config_max_lines_;
    OutputMode current_output_mode_;          // 追踪当前输出模式，避免不必要的切换
};

// --- 日志宏 ---
// 为方便使用，可以定义日志宏。这些宏需要 Logger 实例。
// 如果 Logger 是全局或可静态访问的，则宏可以隐藏实例的传递。
// 此处宏需要显式传递 logger 实例。

#define LSX_LOG_COMMON(logger_instance, level, message) \
    (logger_instance).Log((level), (message), __FILE__, __LINE__, __func__)

#define LSX_LOG_DEBUG(logger_instance, message) \
    LSX_LOG_COMMON(logger_instance, LSX_LIB::Logger::LogLevel::DEBUG, message)

#define LSX_LOG_INFO(logger_instance, message) \
    LSX_LOG_COMMON(logger_instance, LSX_LIB::Logger::LogLevel::INFO, message)

#define LSX_LOG_WARNING(logger_instance, message) \
    LSX_LOG_COMMON(logger_instance, LSX_LIB::Logger::LogLevel::WARNING, message)

#define LSX_LOG_ERROR(logger_instance, message) \
    LSX_LOG_COMMON(logger_instance, LSX_LIB::Logger::LogLevel::ERROR, message)


} // namespace Logger
} // namespace LSX_LIB

#endif // LSX_LIB_LOGGER_LOGGER_H