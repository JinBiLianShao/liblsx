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
        file_stream_.flush();
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
        std::cerr << "[LIBLSX] Error: Could not open log file: " << filepath_ << std::endl;
    }
}

void FileWriter::Write(const std::string& text) {
    if (!file_stream_.is_open()) {
        OpenFile();
        if (!file_stream_.is_open()) {
            std::cerr << "[LIBLSX] Error: Log file " << filepath_ << " is not open. Cannot write log." << std::endl;
            return;
        }
    }

    line_buffer_.push_back(text);

    // 仅在缓冲区刚超过上限时才重写整个文件（丢弃最旧一行）；
    // 未超限的行直接追加写入，避免旧实现每写一行都 trunc + 重写全部的 O(n^2) 磁盘 I/O。
    if (line_buffer_.size() > static_cast<size_t>(max_lines_)) {
        line_buffer_.pop_front();
        RotateLogs();
        return;
    }
    file_stream_ << text << '\n';
    file_stream_.flush();
}

void FileWriter::Flush() {
    // 行已在 Write 时即时写入，这里只需刷新缓冲，不再截断重写整个文件。
    if (file_stream_.is_open()) {
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
        std::cerr << "[LIBLSX] Error: Could not open log file for rotation: " << filepath_ << std::endl;
        return;
    }

    for (const auto& line : line_buffer_) {
        file_stream_ << line << '\n'; // 使用 '\n' 而非 std::endl 以避免不必要的刷新
    }
    file_stream_.flush(); // 在写入所有缓冲行后手动刷新
    // 轮转结束后切回追加模式，后续 Write 直接追加而不会覆写已有行
    file_stream_.close();
    file_stream_.open(filepath_, std::ios::out | std::ios::app);
}

} // namespace Logger
} // namespace LSX_LIB