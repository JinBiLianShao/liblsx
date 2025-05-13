/**
 * @file LogWriter.h
 * @brief 数据传输工具库 - 日志模块输出器接口及实现
 * @details 定义了 LSX_LIB::Logger 命名空间下的日志输出器接口 LogWriter
 * 以及其两个具体实现类 ConsoleWriter (控制台输出) 和 FileWriter (文件输出)。
 * LogWriter 接口定义了日志输出的基本行为 (`Write`, 可选 `Flush`)。
 * ConsoleWriter 将格式化后的日志直接输出到标准输出。
 * FileWriter 将格式化后的日志写入指定文件，并支持基于行数的文件大小限制和轮转。
 * 这些输出器通常由 Logger 类内部使用，负责将最终的日志字符串发送到目标。
 * @author 连思鑫（liansixin）
 * @date 2025年5月13日
 * @version 1.0
 *
 * ### 核心功能
 * - **抽象输出接口**: 定义日志输出的标准接口，支持多态。
 * - **控制台输出**: ConsoleWriter 实现将日志写入控制台。
 * - **文件输出**: FileWriter 实现将日志写入文件，支持文件路径和最大行数配置。
 * - **文件轮转**: FileWriter 在达到最大行数时，会将当前文件内容清空（或进行更复杂的轮转逻辑，此处实现为清空）并重新开始写入。
 * - **刷新**: FileWriter 提供 Flush 方法，强制将缓冲区内容写入文件。
 *
 * ### 使用示例
 *
 * @code
 * #include "LogWriter.h" // 包含 LogWriter 接口和实现类定义
 * #include <iostream>
 * #include <vector>
 * #include <thread>
 * #include <chrono>
 * #include <memory> // For std::unique_ptr
 *
 * // 示例：直接使用 ConsoleWriter
 * void use_console_writer() {
 * LSX_LIB::Logger::ConsoleWriter console_writer;
 * console_writer.Write("This message goes to the console.\n");
 * }
 *
 * // 示例：直接使用 FileWriter
 * void use_file_writer() {
 * // 创建一个 FileWriter，写入到 "test_log.txt"，最大 5 行
 * LSX_LIB::Logger::FileWriter file_writer("test_log.txt", 5);
 *
 * std::cout << "Writing to file..." << std::endl;
 * for (int i = 0; i < 10; ++i) {
 * file_writer.Write("Line " + std::to_string(i) + "\n");
 * std::this_thread::sleep_for(std::chrono::milliseconds(10)); // 模拟写入间隔
 * }
 *
 * file_writer.Flush(); // 强制刷新缓冲区到文件
 * std::cout << "Finished writing to file. Check test_log.txt" << std::endl;
 * // 文件内容应该只有最后 5 行 (取决于 RotateLogs 的具体实现)
 * }
 *
 * int main() {
 * use_console_writer();
 * use_file_writer();
 *
 * // 通常 LogWriter 的实例由 Logger 类管理和使用
 * // 例如在 Logger::Log 方法内部:
 * // writer_->Write(formatted_message);
 *
 * std::cout << "主线程退出。" << std::endl;
 * return 0;
 * }
 * @endcode
 *
 * ### 注意事项
 * - **抽象基类**: LogWriter 是一个抽象基类，不能直接实例化。
 * - **Logger 内部使用**: 这些输出器通常由 Logger 类内部根据配置动态创建和管理。应用程序代码一般通过 Logger 接口进行日志记录。
 * - **线程安全**: LogWriter 接口本身不保证线程安全。具体的实现类（ConsoleWriter, FileWriter）需要在其 Write 方法内部处理线程安全问题。在 FileWriter 中，对文件流和行缓冲区的并发访问需要保护（虽然 Logger 类通常会提供外部锁）。
 * - **文件轮转实现**: 示例代码中的 FileWriter 使用 std::deque 作为行缓冲区，并在达到最大行数时调用 `RotateLogs`。`RotateLogs` 的具体实现（如清空文件、重命名旧文件等）决定了日志轮转的行为。当前示例注释中描述的是清空文件。
 * - **Flush**: Flush 方法对于文件输出很重要，确保缓冲的数据被写入磁盘。在程序正常退出前或重要事件后应考虑调用。
 */

#ifndef LSX_LIB_LOGGER_LOG_WRITER_H
#define LSX_LIB_LOGGER_LOG_WRITER_H
#pragma once
#include <string>
#include <fstream>
#include <iostream>
#include <vector>
#include <deque>
#include <mutex>
#include <utility>


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
         * @brief 抽象日志输出器接口。
         * 定义了日志输出的基本行为，是一个纯虚类。
         * 具体的输出目标（控制台、文件等）需要实现此接口。
         */
        // 抽象输出器接口
        class LogWriter {
        public:
            /**
             * @brief 虚析构函数。
             * 确保通过基类指针删除派生类对象时，能够正确调用派生类的析构函数，释放所有资源。
             */
            virtual ~LogWriter() = default;
            /**
             * @brief 写入日志文本。
             * 纯虚函数，派生类必须实现此方法来将格式化后的日志文本输出到目标。
             *
             * @param text 格式化后的日志文本字符串。
             */
            virtual void Write(const std::string& text) = 0;
            /**
             * @brief 刷新日志缓冲区（可选）。
             * 虚函数，派生类可以重写此方法来强制将缓冲的数据写入目标（例如，刷新文件流）。
             * 默认实现为空操作。
             */
            virtual void Flush() {}; // 可选的 Flush 方法
        };

        /**
         * @brief 控制台日志输出器。
         * LogWriter 接口的具体实现，将日志文本输出到标准控制台。
         */
        // 控制台输出器
        class ConsoleWriter : public LogWriter {
        public:
            /**
             * @brief 默认构造函数。
             * 初始化 ConsoleWriter 对象。
             */
            ConsoleWriter() = default;
            /**
             * @brief 写入日志文本到控制台。
             * 实现 LogWriter::Write 方法，将文本输出到标准输出 (std::cout)。
             *
             * @param text 格式化后的日志文本字符串。
             */
            void Write(const std::string& text) override;
            // ConsoleWriter 通常不需要 Flush，因为控制台输出通常是立即的
        };

        /**
         * @brief 文件日志输出器。
         * LogWriter 接口的具体实现，将日志文本写入指定文件。
         * 支持基于行数的文件大小限制和简单的文件轮转。
         */
        // 文件输出器
        class FileWriter : public LogWriter {
        public:
            /**
             * @brief 构造函数。
             * 初始化 FileWriter 对象，尝试打开指定的日志文件。
             *
             * @param filepath 日志文件的路径。
             * @param max_lines 日志文件的最大行数。达到此行数时会触发轮转。
             */
            FileWriter(const std::string& filepath, int max_lines);
            /**
             * @brief 析构函数。
             * 在对象销毁时自动调用 Flush()，确保所有缓冲的日志都被写入文件，并关闭文件流。
             */
            ~FileWriter() override;

            /**
             * @brief 写入日志文本到文件。
             * 实现 LogWriter::Write 方法，将文本添加到内部缓冲区，并在缓冲区达到一定条件时写入文件。
             * 如果达到最大行数，会触发文件轮转。
             *
             * @param text 格式化后的日志文本字符串。
             */
            void Write(const std::string& text) override;
            /**
             * @brief 刷新文件缓冲区。
             * 实现 LogWriter::Flush 方法，强制将内部缓冲区的内容写入文件，并刷新文件流。
             */
            void Flush() override;

        private:
            /**
             * @brief 日志文件的路径。
             */
            std::string filepath_;
            /**
             * @brief 日志文件的最大行数。
             * 达到此行数时会触发文件轮转。
             */
            int max_lines_;
            /**
             * @brief 输出文件流。
             * 用于将日志文本写入文件。
             */
            std::ofstream file_stream_;
            /**
             * @brief 行缓冲区。
             * 使用 deque 实现一个简单的环形缓冲区，用于存储最近的日志行，以便在达到最大行数时进行轮转。
             */
            std::deque<std::string> line_buffer_; // 用于实现行数限制和轮转

            /**
             * @brief 打开日志文件。
             * 尝试以追加模式打开日志文件。
             */
            void OpenFile();
            /**
             * @brief 轮转日志文件。
             * 当日志行数达到 max_lines_ 时调用。
             * 此处实现通常是将 line_buffer_ 中的内容写入文件，并可能清空文件或重命名旧文件。
             * (具体轮转逻辑取决于实现细节，例如简单清空文件或更复杂的编号/日期轮转)。
             */
            void RotateLogs(); // 将缓冲区内容写入文件
        };

    } // namespace Logger
} // namespace LSX_LIB

#endif // LSX_LIB_LOGGER_LOG_WRITER_H
