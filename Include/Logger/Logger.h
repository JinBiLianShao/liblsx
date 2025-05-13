/**
 * @file Logger.h
 * @brief 数据传输工具库 - 日志模块核心类
 * @details 定义了 LSX_LIB::Logger 命名空间下的 Logger 类，
 * 这是日志模块的核心类，负责日志消息的接收、过滤、格式化和输出。
 * 它使用 LogFormatter 进行消息格式化，使用 LogWriter 进行实际输出（控制台或文件），
 * 并根据配置的日志级别过滤消息。支持动态更改日志级别和输出模式。
 * 类内部使用互斥锁和原子变量保证线程安全。
 * 提供了方便使用的日志宏。
 * @author 连思鑫（liansixin）
 * @date 2025年5月13日
 * @version 1.0
 *
 * ### 核心功能
 * - **日志记录**: 接收不同级别的日志消息。
 * - **级别过滤**: 根据当前配置的日志级别过滤低于该级别的消息。
 * - **消息格式化**: 使用 LogFormatter 将消息格式化为字符串。
 * - **输出管理**: 使用 LogWriter 将格式化后的消息输出到控制台或文件。
 * - **动态配置**: 支持在运行时更改日志级别和输出模式。
 * - **线程安全**: 使用互斥锁和原子变量保护内部状态和输出操作。
 * - **日志宏**: 提供方便的宏简化日志记录调用。
 *
 * ### 使用示例
 *
 * @code
 * #include "Logger.h" // 包含 Logger 类定义
 * #include <iostream>
 * #include <thread>
 * #include <vector>
 * #include <chrono>
 *
 * // 全局或静态的 Logger 实例 (示例)
 * // 在实际应用中，需要确保 Logger 实例的生命周期覆盖所有日志记录点
 * // LSX_LIB::Logger::Logger g_logger(LSX_LIB::Logger::LoggerConfig(
 * //     LSX_LIB::Logger::LogLevel::DEBUG,
 * //     LSX_LIB::Logger::OutputMode::Console,
 * //     "app.log",
 * //     1000
 * // ));
 *
 * // 示例函数，使用日志宏记录日志
 * void worker_function(int id, LSX_LIB::Logger::Logger& logger) {
 * LSX_LOG_INFO(logger, "Worker thread " + std::to_string(id) + " started.");
 *
 * for (int i = 0; i < 3; ++i) {
 * LSX_LOG_DEBUG(logger, "Worker thread " + std::to_string(id) + " processing item " + std::to_string(i) + ".");
 * std::this_thread::sleep_for(std::chrono::milliseconds(50));
 * }
 *
 * LSX_LOG_WARNING(logger, "Worker thread " + std::to_string(id) + " finished processing.");
 * }
 *
 * int main() {
 * // 创建 Logger 实例
 * LSX_LIB::Logger::LoggerConfig config(
 * LSX_LIB::Logger::LogLevel::DEBUG, // 设置初始级别为 DEBUG
 * LSX_LIB::Logger::OutputMode::Console, // 设置初始模式为 Console
 * "my_app.log", // 文件路径 (即使是 Console 模式也会保存)
 * 500 // 文件最大行数
 * );
 * LSX_LIB::Logger::Logger main_logger(config);
 *
 * LSX_LOG_INFO(main_logger, "Application main started.");
 * LSX_LOG_DEBUG(main_logger, "This is a debug message from main.");
 *
 * // 启动工作线程
 * std::vector<std::thread> workers;
 * for (int i = 0; i < 2; ++i) {
 * workers.emplace_back(worker_function, i, std::ref(main_logger)); // 传递 logger 引用
 * }
 *
 * // 动态更改日志级别和模式
 * std::this_thread::sleep_for(std::chrono::milliseconds(200));
 * LSX_LOG_INFO(main_logger, "Changing log level to WARNING.");
 * main_logger.SetLogLevel(LSX_LIB::Logger::LogLevel::WARNING);
 * LSX_LOG_DEBUG(main_logger, "This debug message should NOT appear."); // 应被过滤
 * LSX_LOG_WARNING(main_logger, "This warning message SHOULD appear."); // 应输出
 *
 * std::this_thread::sleep_for(std::chrono::milliseconds(200));
 * LSX_LOG_INFO(main_logger, "Changing output mode to File.");
 * main_logger.SetOutputMode(LSX_LIB::Logger::OutputMode::File);
 * LSX_LOG_ERROR(main_logger, "This error message should go to file."); // 应输出到文件
 *
 * // 等待工作线程完成
 * for (auto& t : workers) {
 * t.join();
 * }
 *
 * LSX_LOG_INFO(main_logger, "Application main finished.");
 *
 * // Logger 对象在 main 函数结束时自动销毁，会调用 Flush()
 * // 或者可以显式调用 Flush()
 * // main_logger.Flush();
 *
 * std::cout << "主线程退出。" << std::endl;
 * return 0;
 * }
 * @endcode
 *
 * ### 注意事项
 * - **Logger 实例**: 在应用程序中通常只需要一个 Logger 实例。可以考虑使用单例模式来管理 Logger 的生命周期和全局访问。
 * - **线程安全**: Logger 类本身是线程安全的，多个线程可以同时调用其 `Log` 方法。
 * - **LogWriter 生命周期**: Logger 类通过 `std::unique_ptr<LogWriter>` 管理 LogWriter 的生命周期。切换输出模式时，会创建新的 LogWriter 实例。
 * - **文件模式**: 在文件模式下，需要确保 Logger 对象在程序完全退出前被销毁或显式调用 `Flush()`，以确保所有缓冲的日志都被写入文件。
 * - **日志宏**: 提供的日志宏方便使用，但需要确保能够访问到 Logger 实例。如果使用全局或静态 Logger 实例，宏可以简化为 `LSX_LOG_INFO("message")` 形式。
 * - **性能**: 日志记录操作（特别是文件输出）可能涉及I/O操作，在高频率日志记录场景下可能影响性能。可以考虑异步日志或批量写入等优化。
 */

#ifndef LSX_LIB_LOGGER_LOGGER_H
#define LSX_LIB_LOGGER_LOGGER_H
#pragma once
#include "LogCommon.h" // 包含日志通用定义 (LogLevel, OutputMode, LoggerConfig)
#include "LogFormatter.h" // 包含 LogFormatter 类定义
#include "LogWriter.h" // 包含 LogWriter 类定义
#include <string> // 包含 std::string
#include <memory> // 包含 std::unique_ptr
#include <mutex> // 包含 std::mutex, std::lock_guard
#include <atomic> // 包含 std::atomic
#include <utility> // 包含 std::move (用于 LoggerConfig 构造函数)


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
         * @brief 日志模块核心类。
         * 负责日志消息的接收、过滤、格式化和输出。
         * 管理 LogFormatter 和 LogWriter 的生命周期和使用。
         * 支持动态配置日志级别和输出模式。
         */
        class Logger {
        public:
            /**
             * @brief 构造函数。
             * 使用指定的日志配置初始化 Logger 对象。
             * 根据配置创建初始的 LogWriter 实例。
             *
             * @param config 日志配置结构体。
             */
            explicit Logger(const LoggerConfig& config);

            /**
             * @brief 析构函数。
             * 在 Logger 对象销毁时自动调用 Flush()，确保所有缓冲的日志都被写入。
             * 同时负责释放 LogWriter 资源。
             */
            ~Logger() = default;

            // 禁止拷贝和赋值，Logger 对象管理 LogWriter 资源和状态，拷贝不安全且无意义。
            /**
             * @brief 禁用拷贝构造函数。
             */
            Logger(const Logger&) = delete;
            /**
             * @brief 禁用赋值运算符。
             */
            Logger& operator=(const Logger&) = delete;
            /**
             * @brief 禁用移动构造函数。
             * 虽然理论上可以实现移动，但考虑到通常 Logger 是单例或静态的，禁用移动可简化设计。
             */
            Logger(Logger&&) = delete;
            /**
             * @brief 禁用移动赋值运算符。
             * 禁用移动赋值。
             */
            Logger& operator=(Logger&&) = delete;

            /**
             * @brief 记录日志。
             * 根据此条消息的日志级别和当前配置的日志级别进行过滤。
             * 如果消息级别满足要求，则使用 LogFormatter 格式化消息，并使用当前的 LogWriter 进行输出。
             * 此方法是线程安全的。
             *
             * @param msg_level 此条消息的日志级别。
             * @param msg 日志消息体字符串。
             * @param file 源代码文件名 (通常使用 __FILE__ 宏)。
             * @param line 源代码行号 (通常使用 __LINE__ 宏)。
             * @param func 源代码函数名 (通常使用 __func__ 或 __FUNCTION__ 宏)。
             */
            void Log(LogLevel msg_level,
                     const std::string& msg,
                     const char* file,
                     int line,
                     const char* func);

            /**
             * @brief 动态设置日志输出模式。
             * 切换日志消息的输出目标。如果模式发生变化，会创建新的 LogWriter 实例并替换当前的。
             * 此方法是线程安全的。
             *
             * @param mode 新的输出模式 (Console 或 File)。
             */
            void SetOutputMode(OutputMode mode);

            /**
             * @brief 获取当前输出模式。
             *
             * @return 当前的输出模式。
             */
            OutputMode GetOutputMode() const;

            /**
             * @brief 动态设置日志记录级别。
             * 更改 Logger 将处理和输出的最低日志级别。低于此级别的日志消息将被忽略。
             * 此方法是线程安全的。
             *
             * @param new_level 新的日志级别。
             */
            void SetLogLevel(LogLevel new_level);

            /**
             * @brief 获取当前日志记录级别。
             *
             * @return 当前的日志级别。
             */
            LogLevel GetLogLevel() const;

            /**
             * @brief 手动刷新日志（主要用于文件输出）。
             * 强制当前的 LogWriter 将所有缓冲的日志写入目标（例如，刷新文件缓冲区）。
             * 在程序退出前或重要操作后调用此方法可以确保日志不会丢失。
             * 此方法是线程安全的。
             */
            void Flush();


        private:
            /**
             * @brief 当前日志记录级别。
             * 原子变量，用于线程安全地读取和修改当前的日志过滤级别。
             */
            std::atomic<LogLevel> current_log_level_; // 当前日志记录级别
            /**
             * @brief 日志格式化器实例。
             * 用于将日志消息格式化为字符串。
             */
            LogFormatter formatter_;                  // 日志格式化器
            /**
             * @brief 当前日志输出器。
             * 智能指针管理 LogWriter 的生命周期。根据输出模式指向 ConsoleWriter 或 FileWriter 实例。
             */
            std::unique_ptr<LogWriter> writer_;       // 当前日志输出器
            /**
             * @brief 互斥锁。
             * 用于保护 writer_ 的创建/切换以及对 LogWriter 实例的写入操作，确保线程安全。
             */
            mutable std::mutex writer_mutex_;         //保护 writer_ 的创建/切换以及写入操作

            // 从初始配置中保存的文件相关设置，用于在切换到文件模式时使用
            /**
             * @brief 配置的日志文件路径。
             * 从初始 LoggerConfig 中保存，用于在切换到文件输出模式时创建 FileWriter。
             */
            std::string config_filepath_;
            /**
             * @brief 配置的文件最大行数。
             * 从初始 LoggerConfig 中保存，用于在文件输出模式下控制文件大小。
             */
            int config_max_lines_;
            /**
             * @brief 追踪当前输出模式。
             * 用于判断是否需要切换 LogWriter 实例。
             */
            OutputMode current_output_mode_;          // 追踪当前输出模式，避免不必要的切换
        };

        // --- 日志宏 ---
        // 为方便使用，可以定义日志宏。这些宏需要 Logger 实例。
        // 如果 Logger 是全局或可静态访问的，则宏可以隐藏实例的传递。
        // 此处宏需要显式传递 logger 实例。

        /**
         * @brief 通用日志记录宏。
         * 这是其他级别日志宏的基础。根据指定的日志级别和消息，调用 Logger 实例的 Log 方法。
         * 自动捕获源代码文件名、行号和函数名。
         *
         * @param logger_instance Logger 类的实例。
         * @param level 日志级别 (LogLevel 枚举值)。
         * @param message 日志消息体字符串。
         */
#define LSX_LOG_COMMON(logger_instance, level, message) \
            (logger_instance).Log((level), (message), __FILE__, __LINE__, __func__)

        /**
         * @brief 记录 DEBUG 级别日志的宏。
         *
         * @param logger_instance Logger 类的实例。
         * @param message 日志消息体字符串。
         */
#define LSX_LOG_DEBUG(logger_instance, message) \
            LSX_LOG_COMMON(logger_instance, LSX_LIB::Logger::LogLevel::DEBUG, message)

        /**
         * @brief 记录 INFO 级别日志的宏。
         *
         * @param logger_instance Logger 类的实例。
         * @param message 日志消息体字符串。
         */
#define LSX_LOG_INFO(logger_instance, message) \
            LSX_LOG_COMMON(logger_instance, LSX_LIB::Logger::LogLevel::INFO, message)

        /**
         * @brief 记录 WARNING 级别日志的宏。
         *
         * @param logger_instance Logger 类的实例。
         * @param message 日志消息体字符串。
         */
#define LSX_LOG_WARNING(logger_instance, message) \
            LSX_LOG_COMMON(logger_instance, LSX_LIB::Logger::LogLevel::WARNING, message)

        /**
         * @brief 记录 ERROR 级别日志的宏。
         *
         * @param logger_instance Logger 类的实例。
         * @param message 日志消息体字符串。
         */
#define LSX_LOG_ERROR(logger_instance, message) \
            LSX_LOG_COMMON(logger_instance, LSX_LIB::Logger::LogLevel::ERROR, message)


    } // namespace Logger
} // namespace LSX_LIB

#endif // LSX_LIB_LOGGER_LOGGER_H
