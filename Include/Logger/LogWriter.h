#ifndef LSX_LIB_LOGGER_LOG_WRITER_H
#define LSX_LIB_LOGGER_LOG_WRITER_H
#pragma once
#include <string>
#include <fstream>  // For std::ofstream
#include <iostream> // For std::cout, std::cerr
#include <vector>   // For FileWriter line buffer
#include <deque>    // For FileWriter line buffer (ring buffer)
#include <mutex>    // For FileWriter internal operations if needed (though Logger provides primary lock)

namespace LSX_LIB {
    namespace Logger {

        // 抽象输出器接口
        class LogWriter {
        public:
            virtual ~LogWriter() = default;
            virtual void Write(const std::string& text) = 0;
            virtual void Flush() {}; // 可选的 Flush 方法
        };

        // 控制台输出器
        class ConsoleWriter : public LogWriter {
        public:
            ConsoleWriter() = default;
            void Write(const std::string& text) override;
        };

        // 文件输出器
        class FileWriter : public LogWriter {
        public:
            FileWriter(const std::string& filepath, int max_lines);
            ~FileWriter() override;

            void Write(const std::string& text) override;
            void Flush() override;

        private:
            std::string filepath_;
            int max_lines_;
            std::ofstream file_stream_;
            std::deque<std::string> line_buffer_; // 用于实现行数限制和轮转

            void OpenFile();
            void RotateLogs(); // 将缓冲区内容写入文件
        };

    } // namespace Logger
} // namespace LSX_LIB

#endif // LSX_LIB_LOGGER_LOG_WRITER_H