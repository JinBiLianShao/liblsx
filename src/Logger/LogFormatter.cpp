#include "LogFormatter.h"

#include <cstring>
#include <ctime> // For std::localtime

// C++17 <filesystem> for path manipulation, but for __FILE__ we just need the basename.
// For simplicity, we can implement a basic basename extractor.
#if __cplusplus >= 201703L && __has_include(<filesystem>)
#include <filesystem>
namespace fs = std::filesystem;
#endif

namespace LSX_LIB {
namespace Logger {

// 提取文件名（不含路径）的辅助函数
static std::string GetBaseFileName(const char* filepath) {
    if (filepath == nullptr) {
        return "UnknownFile";
    }
#if __cplusplus >= 201703L && __has_include(<filesystem>)
    // 使用 C++17 filesystem 获取文件名
    try {
        return fs::path(filepath).filename().string();
    } catch (...) {
        // Fallback in case of filesystem error
    }
#endif
    // 简单的手动实现，查找最后一个路径分隔符
    const char* last_slash = strrchr(filepath, '/');
    const char* last_backslash = strrchr(filepath, '\\');
    const char* base_name_ptr = filepath;

    if (last_slash) {
        base_name_ptr = last_slash + 1;
    }
    if (last_backslash && last_backslash + 1 > base_name_ptr) {
        base_name_ptr = last_backslash + 1;
    }
    return std::string(base_name_ptr);
}


std::string LogFormatter::Format(LogLevel level,
                                 const std::string& msg,
                                 const char* file,
                                 int line,
                                 const char* func) {
    // 1. 时间戳
    auto now_sys_clock = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now_sys_clock);
    // 使用 std::localtime。注意：std::localtime 不是线程安全的，但在Logger的Log方法中，
    //整个格式化和写入过程会被互斥锁保护，因此在这里是安全的。
    // 如果需要极致性能且格式化在锁外进行，应使用平台特定的线程安全版本（如 localtime_s, localtime_r）。
    std::tm now_tm;
#ifdef _WIN32
    localtime_s(&now_tm, &now_time_t);
#else
    localtime_r(&now_time_t, &now_tm); // POSIX
#endif

    // 获取毫秒部分
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now_sys_clock.time_since_epoch()) % 1000;

    std::ostringstream oss;
    oss << "[" << std::put_time(&now_tm, "%Y-%m-%d %H:%M:%S") 
        << "." << std::setfill('0') << std::setw(3) << ms.count() << "]";

    // 2. 线程ID
    oss << "[Thread " << std::this_thread::get_id() << "]";

    // 3. 日志级别
    oss << "[" << LogLevelToString(level) << "]";

    // 4. 文件名、行号、函数名
    oss << "[" << GetBaseFileName(file) << ":" << line << " (" << (func ? func : "UnknownFunc") << ")] ";
    
    // 5. 日志消息
    oss << msg;

    return oss.str();
}

std::string LogFormatter::LogLevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG:   return "DEBUG";
        case LogLevel::INFO:    return " INFO"; // 保持长度一致性
        case LogLevel::WARNING: return " WARN";
        case LogLevel::ERROR:   return "ERROR";
        default:                return " UNKN";
    }
}

} // namespace Logger
} // namespace LSX_LIB