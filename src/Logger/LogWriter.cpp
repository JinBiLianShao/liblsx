#include "LogWriter.h"
#include "LogCommon.h" // 主要为了调试时可能用到的 std::cerr

namespace LSX_LIB {
namespace Logger {

// --- ConsoleWriter 实现 ---
void ConsoleWriter::Write(const std::string& text) {
    // 可以在此根据日志级别选择 std::cout 或 std::cerr，
    // 但当前设计中，格式化后的字符串已包含级别信息，直接输出即可。
    // 为简单起见，全部输出到 std::cout。
    // std::endl 会刷新缓冲区，对于控制台是可接受的。
    std::cout << text << std::endl;
}

// --- FileWriter 实现 ---
FileWriter::FileWriter(const std::string& filepath, int max_lines)
    : filepath_(filepath), max_lines_(max_lines > 0 ? max_lines : 1) { // 保证 max_lines 至少为1
    OpenFile(); // 尝试打开或创建文件
    // 如果需要，可以在这里从现有文件中加载最后 N 行到 line_buffer_
    // 为了简化，每次程序启动时，如果文件存在，它将被用于追加，
    // 直到行数达到限制并发生截断/重写。
    // 或者，如果希望总是从一个干净的 N 行文件开始，可以在 OpenFile 中处理。
    // 当前的 RotateLogs() 采用的是覆写模式。
}

FileWriter::~FileWriter() {
    if (file_stream_.is_open()) {
        RotateLogs(); // 确保退出前将缓冲区剩余内容写入
        file_stream_.close();
    }
}

void FileWriter::OpenFile() {
    // 以输出模式打开文件，如果文件不存在则创建。
    // 由于 RotateLogs 会截断并重写，这里的初始模式可以是 out 或 app。
    // 如果是 app，则会追加到现有内容，直到 RotateLogs 被触发。
    // 如果是 out，则每次启动会清空。
    // 我们采用每次 Write 时判断并 rotate 的方式，所以初始打开即可。
    file_stream_.open(filepath_, std::ios::out | std::ios::app); // 追加模式打开，RotateLogs会处理截断
    if (!file_stream_.is_open()) {
        std::cerr << "Error: Could not open log file: " << filepath_ << std::endl;
    }
}

void FileWriter::Write(const std::string& text) {
    if (!file_stream_.is_open()) {
        // 尝试重新打开文件，如果之前打开失败
        OpenFile();
        if (!file_stream_.is_open()) {
            std::cerr << "Error: Log file " << filepath_ << " is not open. Cannot write log." << std::endl;
            return; // 无法写入，直接返回
        }
    }

    line_buffer_.push_back(text);

    // 当缓冲区大小超过最大行数时，移除最旧的行
    while (line_buffer_.size() > static_cast<size_t>(max_lines_)) {
        line_buffer_.pop_front();
    }
    
    // 将日志实际写入文件（此处简化为每次都重写，符合循环缓冲区的描述）
    // RotateLogs 负责将 line_buffer_ 的内容刷新到文件，并确保文件只包含这些行。
    RotateLogs();
}

void FileWriter::Flush() {
    if (file_stream_.is_open()) {
        RotateLogs(); // 确保缓冲区内容写入
        file_stream_.flush();
    }
}

void FileWriter::RotateLogs() {
    if (!file_stream_.is_open() && !filepath_.empty()) {
        // 如果流未打开但文件路径有效，尝试打开它
        // 这通常不应发生，因为 Write 会检查并尝试打开
        file_stream_.open(filepath_, std::ios::out | std::ios::trunc);
    } else if (file_stream_.is_open()) {
        // 文件已打开，关闭并以截断模式重新打开以覆写
        file_stream_.close();
        file_stream_.open(filepath_, std::ios::out | std::ios::trunc);
    }
    
    if (!file_stream_.is_open()) {
        std::cerr << "Error: Could not open log file for rotation: " << filepath_ << std::endl;
        return;
    }

    for (const auto& line : line_buffer_) {
        file_stream_ << line << '\n'; // 使用 '\n' 而非 std::endl 以避免不必要的刷新
    }
    file_stream_.flush(); // 在写入所有缓冲行后手动刷新
}

} // namespace Logger
} // namespace LSX_LIB