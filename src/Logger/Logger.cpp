#include "Logger.h"
#include <iostream>

namespace LSX_LIB {
namespace Logger {

Logger::Logger(const LoggerConfig& config)
    : current_log_level_(config.level),
      formatter_(),
      writer_(nullptr), // 初始化为空指针
      config_filepath_(config.filepath),
      config_max_lines_(config.maxLines > 0 ? config.maxLines : 1000), // 确保max_lines有效
      current_output_mode_(config.mode) {
    if (config_filepath_.empty() && config.mode == OutputMode::File) {
        std::cerr << "Logger Warning: File output mode selected but no filepath provided. Defaulting to 'app.log'." << std::endl;
        config_filepath_ = "app.log";
    }
    SetOutputMode(config.mode); // 根据初始配置设置输出器
}

void Logger::Log(LogLevel msg_level,
                 const std::string& msg,
                 const char* file,
                 int line,
                 const char* func) {
    // 1. 级别过滤
    // 使用 memory_order_relaxed 因为日志级别的读取不依赖于其他内存操作的严格顺序
    if (msg_level < current_log_level_.load(std::memory_order_relaxed)) {
        return;
    }

    // 2. 格式化日志消息
    std::string formatted_msg = formatter_.Format(msg_level, msg, file, line, func);

    // 3. 输出日志 (线程安全)
    std::lock_guard<std::mutex> lock(writer_mutex_);
    if (writer_) {
        writer_->Write(formatted_msg);
    } else {
        // 备用输出，例如当 writer_ 意外为空时
        std::cerr << "Logger Error: Log writer is not initialized. Message: " << formatted_msg << std::endl;
    }
}

void Logger::SetOutputMode(OutputMode mode) {
    std::lock_guard<std::mutex> lock(writer_mutex_);
    if (mode == current_output_mode_ && writer_ != nullptr) {
        // 如果模式未改变且写入器已存在，则无需操作
        // 注意：如果文件路径等参数也允许动态更改，则需要更复杂的逻辑
        return;
    }

    current_output_mode_ = mode; // 更新当前模式

    // 先释放旧的 writer (unique_ptr 会自动处理)
    writer_.reset();

    if (mode == OutputMode::Console) {
        writer_ = std::make_unique<ConsoleWriter>();
    } else { // OutputMode::File
        if (config_filepath_.empty()) {
             // 这种情况理论上已在构造函数中处理，但作为安全检查
            std::cerr << "Logger Error: Cannot switch to File output mode. Filepath is empty." << std::endl;
            // 可以选择切换回控制台或保持无输出状态
            writer_ = std::make_unique<ConsoleWriter>(); // 回退到控制台
            current_output_mode_ = OutputMode::Console;
            std::cerr << "Logger Warning: Switched back to Console output due to empty filepath." << std::endl;
            return;
        }
        writer_ = std::make_unique<FileWriter>(config_filepath_, config_max_lines_);
    }
}

OutputMode Logger::GetOutputMode() const {
    // current_output_mode_ 不是原子变量，但其读取通常与SetOutputMode（写）互斥
    // 为简单起见，这里直接返回。如果需要严格的线程安全保证，也应加锁或使用原子变量。
    // 但通常获取模式的操作不是性能关键路径或高频并发操作。
    std::lock_guard<std::mutex> lock(writer_mutex_); // 确保读取时模式是稳定的
    return current_output_mode_;
}


void Logger::SetLogLevel(LogLevel new_level) {
    // 使用 memory_order_release 确保此存储对后续 relaxed/acquire 读取可见
    current_log_level_.store(new_level, std::memory_order_release);
}

LogLevel Logger::GetLogLevel() const {
    return current_log_level_.load(std::memory_order_acquire);
}

void Logger::Flush() {
    std::lock_guard<std::mutex> lock(writer_mutex_);
    if (writer_) {
        writer_->Flush();
    }
}

} // namespace Logger
} // namespace LSX_LIB