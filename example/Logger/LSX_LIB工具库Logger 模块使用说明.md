# LSX_LIB::Logger 模块使用说明

## 1. 简介

`LSX_LIB::Logger` 是一个为 C++ 应用程序设计的轻量级、多功能日志模块。它旨在提供一种简单易用且功能丰富的日志记录方案，主要特性包括：

* **多线程安全**：内部使用互斥锁和原子操作确保在多线程环境下的安全写入。
* **跨平台设计**：主要依赖 C++ 标准库，方便在 Linux 和 Windows 等平台上编译和运行。
* **日志级别控制**：支持 DEBUG, INFO, WARNING, ERROR 四种日志级别，并可以动态调整。
* **多种输出目标**：支持输出到控制台 (Console) 和本地文件 (File)。
* **动态输出切换**：可以在运行时动态切换日志的输出目标。
* **日志轮转**：文件输出模式下，支持按最大行数限制进行日志轮转（保留最新的 N 行）。
* **灵活配置**：通过 `LoggerConfig` 结构体在创建 Logger 实例时进行初始化配置。
* **清晰的日志格式**：包含时间戳（精确到毫秒）、线程ID、日志级别、代码位置（文件名、行号、函数名）和日志消息。
* **便捷的宏定义**：提供 `LSX_LOG_DEBUG`, `LSX_LOG_INFO` 等宏，简化日志调用。

## 2. 开始使用

### 2.1 文件结构

模块的主要文件位于 `LSX_LIB/Logger/` 目录下：

```
LSX_LIB/Logger
├── Include/
│   ├── LogCommon.h      // 定义日志级别、输出模式、配置结构体
│   ├── LogFormatter.h   // 日志格式化器声明
│   ├── LogWriter.h      // 日志输出器接口及派生类声明
│   ├── Logger.h         // 日志管理器声明 (主要包含的头文件)
├──  src/
│   ├── LogFormatter.cpp // 日志格式化器实现
│   ├── Logger.cpp       // 日志管理器实现
│   ├── LogWriter.cpp    // ConsoleWriter, FileWriter 实现
```

### 2.2 集成到项目

1.  **包含头文件**：
    在您的 C++源文件中，主要包含 `Logger.h` 即可，它会间接包含其他必要的模块内头文件。

    ```cpp
    #include "LSX_LIB/Logger/Logger.h" // 假设 LSX_LIB 目录在您的包含路径中，或者使用相对路径
    ```

2.  **命名空间**：
    模块的所有组件都位于 `LSX_LIB::Logger` 命名空间下。

    ```cpp
    // 使用 using 指令简化调用
    using namespace LSX_LIB::Logger;
    // 或者显式指定命名空间
    // LSX_LIB::Logger::LoggerConfig config;
    ```

### 2.3 编译

编译时请确保链接所有相关的 `.cpp` 文件，并启用 C++17 (或至少 C++11，但 C++17 用于 `std::filesystem` 等特性会更好，尽管此模块已做兼容处理) 和多线程支持。

**Linux (g++) 示例：**

```bash
g++ -std=c++17 -pthread \
    YourApp.cpp \
    LSX_LIB/Logger/LogFormatter.cpp \
    LSX_LIB/Logger/LogWriter.cpp \
    LSX_LIB/Logger/Logger.cpp \
    -o YourAppExecutable
```

(您可能需要根据项目结构调整路径和编译命令。)

## 3. 核心概念

### 3.1 `LoggerConfig` 结构体

用于在创建 `Logger` 实例时进行初始化配置。

```cpp
struct LoggerConfig {
    LogLevel level = LogLevel::INFO;       // 默认日志级别
    OutputMode mode = OutputMode::Console; // 默认输出模式
    std::string filepath = "app.log";    // 默认日志文件路径 (用于文件输出)
    int maxLines = 1000;                 // 默认文件最大行数 (用于文件输出)

    // 构造函数
    LoggerConfig() = default;
    LoggerConfig(LogLevel lvl, OutputMode m, std::string path, int lines);
};
```

* `level`: 初始化时的日志记录级别。
* `mode`: 初始化时的日志输出模式。
* `filepath`: 如果输出模式为 `File`，此路径指定日志文件的位置。
* `maxLines`: 如果输出模式为 `File`，此参数指定日志文件的最大行数。当超过此行数时，最旧的日志将被丢弃。

### 3.2 `LogLevel` 枚举

定义了四种日志级别，级别越低，数值越小：

* `LogLevel::DEBUG` (0): 用于详细的调试信息。
* `LogLevel::INFO` (1): 用于常规的程序运行信息。
* `LogLevel::WARNING` (2): 用于潜在问题或非严重错误。
* `LogLevel::ERROR` (3): 用于严重错误，可能影响程序正常运行。

当设置某个日志级别后，只有等于或高于该级别的日志才会被记录。例如，设置为 `INFO`，则 `DEBUG` 级别的日志将被忽略。

### 3.3 `OutputMode` 枚举

定义了两种日志输出模式：

* `OutputMode::Console`: 日志输出到标准控制台 (`std::cout`)。
* `OutputMode::File`: 日志输出到通过 `LoggerConfig::filepath` 指定的文件。

## 4. 基本用法

### 4.1 创建 Logger 实例

首先，配置 `LoggerConfig` 对象，然后用它来创建 `Logger` 实例。

```cpp
#include "LSX_LIB/Logger/Logger.h"

int main() {
    LSX_LIB::Logger::LoggerConfig config;
    config.level = LSX_LIB::Logger::LogLevel::DEBUG; // 记录所有 DEBUG 及以上级别的日志
    config.mode = LSX_LIB::Logger::OutputMode::Console; // 输出到控制台
    config.filepath = "my_application.log"; // 如果切换到文件模式，这是文件名
    config.maxLines = 2000;                 // 文件模式下的最大行数

    LSX_LIB::Logger::Logger logger(config); // 创建 Logger 实例

    // ... 开始使用 logger
    return 0;
}
```

### 4.2 使用日志宏记录日志

为了方便记录日志并自动包含代码位置信息，推荐使用提供的日志宏。这些宏需要一个 `Logger` 实例作为第一个参数。

```cpp
// 假设 logger 实例已创建

LSX_LOG_DEBUG(logger, "这是一个调试信息，变量 x = " + std::to_string(some_variable));
LSX_LOG_INFO(logger, "用户登录成功: user_id=" + userId);
LSX_LOG_WARNING(logger, "配置文件部分参数缺失，使用默认值。");
LSX_LOG_ERROR(logger, "数据库连接失败: " + db_error_message);
```

这些宏会自动填充当前文件名 (`__FILE__`)、行号 (`__LINE__`) 和函数名 (`__func__`)。

如果您不想使用宏，也可以直接调用 `Log` 方法：

```cpp
logger.Log(LSX_LIB::Logger::LogLevel::INFO, "手动调用 Log 方法", __FILE__, __LINE__, __func__);
```

## 5. 功能详解与动态控制

### 5.1 动态修改日志级别

可以在程序运行时改变日志的记录级别。

```cpp
// 将日志级别提升到 WARNING，此后 DEBUG 和 INFO 级别的日志将被忽略
logger.SetLogLevel(LSX_LIB::Logger::LogLevel::WARNING);

// 获取当前日志级别
LSX_LIB::Logger::LogLevel currentLevel = logger.GetLogLevel();
if (currentLevel == LSX_LIB::Logger::LogLevel::WARNING) {
    LSX_LOG_INFO(logger, "当前级别是 WARNING (这条INFO不会被记录)"); // 不会输出
}
LSX_LOG_WARNING(logger, "这条 WARNING 会被记录。"); // 会输出
```

### 5.2 动态切换输出模式

可以在控制台输出和文件输出之间动态切换。

```cpp
// 切换到文件输出模式
// 文件路径和最大行数将使用 Logger 构造时 LoggerConfig 中提供的值
logger.SetOutputMode(LSX_LIB::Logger::OutputMode::File);
LSX_LOG_INFO(logger, "这条日志现在会写入文件: " + config.filepath);

// 切换回控制台输出
logger.SetOutputMode(LSX_LIB::Logger::OutputMode::Console);
LSX_LOG_INFO(logger, "这条日志现在会输出到控制台。");

// 获取当前输出模式
LSX_LIB::Logger::OutputMode currentMode = logger.GetOutputMode();
```

### 5.3 文件日志详解 (`FileWriter`)

当输出模式为 `File` 时：

* **`filepath`**: 日志将写入 `LoggerConfig` 中或通过 `Logger` 构造函数设置的 `filepath` 指定的文件。如果文件不存在，会被创建。
* **`maxLines`**: `FileWriter` 内部维护一个行缓冲区（`std::deque`）。当记录的日志行数即将超过 `maxLines` 时，最旧的一行日志会从缓冲区头部移除，新的日志从尾部加入。然后，整个缓冲区的内容会被覆写到日志文件中。这确保日志文件始终只包含最新的 `maxLines` 行日志。
* **`Flush()`**: 您可以调用 `logger.Flush()` 方法来确保所有缓冲的日志（尤其是文件日志）立即写入到目标存储。`FileWriter` 在每次写入（并可能发生轮转）后以及析构时会自动刷新。

    ```cpp
    logger.SetOutputMode(LSX_LIB::Logger::OutputMode::File);
    for (int i = 0; i < 10; ++i) { // 假设 maxLines = 5
        LSX_LOG_INFO(logger, "文件日志行 " + std::to_string(i + 1));
    }
    // 此时 "my_application.log" 文件中应包含 "文件日志行 6" 到 "文件日志行 10"
    logger.Flush(); // 确保所有内容已写入（在此例中，Write内部的RotateLogs已完成）
    ```

### 5.4 日志格式

默认的日志输出格式如下：

`[YYYY-MM-DD HH:MM:SS.mmm][Thread <ID>][<LEVEL>][<filename>:<line> (<function>)] <message>`

* `YYYY-MM-DD HH:MM:SS.mmm`: 年-月-日 时:分:秒.毫秒 (例如: `[2024-05-12 15:30:05.123]`)
* `Thread <ID>`: 当前线程的ID (例如: `[Thread 140123456789012]`)
* `<LEVEL>`: 日志级别，如 `DEBUG`, ` INFO`, ` WARN`, `ERROR` (注意为了对齐，部分级别字符串前后有空格)。
* `<filename>:<line> (<function>)`: 日志发生处的源文件名、行号和函数名 (例如: `[main.cpp:42 (my_function)]`)。文件名是基本名称，不含路径。
* `<message>`: 实际的日志消息内容。

### 5.5 线程安全

`Logger` 模块设计为线程安全的：

* 对日志输出器 (`LogWriter`) 的访问（包括写入日志和切换输出器）通过 `std::mutex`进行保护。
* 日志级别 (`current_log_level_`) 使用 `std::atomic` 进行管理，其读写是原子操作，避免了数据竞争。

因此，您可以在多个线程中共享同一个 `Logger` 实例并安全地记录日志。

## 6. 完整示例参考

```c++
#include "Logger.h" // 会包含 LogCommon.h 等
#include <iostream>
#include <vector>
#include <thread>
#include <chrono> 
#include <fstream>  

// 辅助函数：读取文件最后 N 行 (仅为测试查看，非日志库一部分)
void PrintLastNLines(const std::string& filepath, int n) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cout << "Test Info: Could not open file " << filepath << " to check content." << std::endl;
        return;
    }
    std::cout << "\n--- Checking last lines of " << filepath << " ---" << std::endl;
    std::deque<std::string> lines;
    std::string line;
    while (std::getline(file, line)) {
        lines.push_back(line);
        if (lines.size() > static_cast<size_t>(n)) {
            lines.pop_front();
        }
    }
    for (const auto& l : lines) {
        std::cout << l << std::endl;
    }
    std::cout << "--- End of file check ---" << std::endl;
}


int main() {
    // --- 1. 初始化 Logger ---
    std::cout << "--- Test 1: Initial Console Logging ---" << std::endl;
    LSX_LIB::Logger::LoggerConfig config;
    config.level = LSX_LIB::Logger::LogLevel::DEBUG; // 设置初始级别为 DEBUG
    config.mode = LSX_LIB::Logger::OutputMode::Console;
    config.filepath = "test_app.log"; // 文件名
    config.maxLines = 5;             // 文件输出时，最大行数设置为5，方便测试轮转

    LSX_LIB::Logger::Logger logger(config);

    // 使用宏进行日志记录
    LSX_LOG_DEBUG(logger, "This is a debug message.");
    LSX_LOG_INFO(logger, "Application starting...");
    LSX_LOG_WARNING(logger, "This is a warning message.");
    LSX_LOG_ERROR(logger, "An error occurred!");

    // --- 2. 动态修改日志级别 ---
    std::cout << "\n--- Test 2: Changing Log Level to WARNING ---" << std::endl;
    logger.SetLogLevel(LSX_LIB::Logger::LogLevel::WARNING);
    std::cout << "Current log level: "
              << static_cast<int>(logger.GetLogLevel()) // INFO: 0 DEBUG, 1 INFO, 2 WARNING, 3 ERROR
              << " (WARNING)" << std::endl;

    LSX_LOG_DEBUG(logger, "This debug message should NOT be seen.");
    LSX_LOG_INFO(logger, "This info message should NOT be seen.");
    LSX_LOG_WARNING(logger, "This warning (after level change) SHOULD be seen.");
    LSX_LOG_ERROR(logger, "This error (after level change) SHOULD be seen.");

    // 恢复 DEBUG 级别用于后续测试
    logger.SetLogLevel(LSX_LIB::Logger::LogLevel::DEBUG);
    std::cout << "Log level reset to DEBUG." << std::endl;

    // --- 3. 动态切换到文件输出 ---
    std::cout << "\n--- Test 3: Switching to File Output (maxLines=5) ---" << std::endl;
    logger.SetOutputMode(LSX_LIB::Logger::OutputMode::File);
    std::cout << "Current output mode: "
              << (logger.GetOutputMode() == LSX_LIB::Logger::OutputMode::File ? "File" : "Console")
              << std::endl;

    LSX_LOG_INFO(logger, "File Log: Message 1");
    LSX_LOG_INFO(logger, "File Log: Message 2");
    LSX_LOG_INFO(logger, "File Log: Message 3");
    LSX_LOG_INFO(logger, "File Log: Message 4");
    LSX_LOG_INFO(logger, "File Log: Message 5");
    LSX_LOG_INFO(logger, "File Log: Message 6 (this should cause oldest to be removed)");
    LSX_LOG_INFO(logger, "File Log: Message 7 (and this too)");
    
    logger.Flush(); // 确保所有内容已写入文件
    PrintLastNLines(config.filepath, config.maxLines + 2); // 打印文件内容检查，多打几行以防万一

    // --- 4. 切换回控制台输出 ---
    std::cout << "\n--- Test 4: Switching back to Console Output ---" << std::endl;
    logger.SetOutputMode(LSX_LIB::Logger::OutputMode::Console);
    LSX_LOG_INFO(logger, "Switched back to console. This message should be on console.");


    // --- 5. 多线程测试 ---
    std::cout << "\n--- Test 5: Multithreading Test (logging to console) ---" << std::endl;
    // 为了清晰，切换回控制台，或者可以再切换到一个新的日志文件
    // logger.SetOutputMode(LSX_LIB::Logger::OutputMode::Console);

    std::vector<std::thread> threads;
    int num_threads = 3;
    int messages_per_thread = 4;

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&logger, i, messages_per_thread]() {
            for (int j = 0; j < messages_per_thread; ++j) {
                std::string msg = "Thread " + std::to_string(i) + " says: Hello! Count " + std::to_string(j);
                LSX_LOG_INFO(logger, msg);
                std::this_thread::sleep_for(std::chrono::milliseconds(5)); // 短暂休眠模拟工作
            }
        });
    }

    for (auto& t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }
    std::cout << "Multithreading test finished." << std::endl;

    // 测试文件输出的线程安全性 (可选，可以配置一个新的文件)
    std::cout << "\n--- Test 6: Multithreading Test (logging to file 'multithread_test.log') ---" << std::endl;
    LSX_LIB::Logger::LoggerConfig mt_file_config;
    mt_file_config.level = LSX_LIB::Logger::LogLevel::DEBUG;
    mt_file_config.mode = LSX_LIB::Logger::OutputMode::File;
    mt_file_config.filepath = "multithread_app.log";
    mt_file_config.maxLines = 10; // (num_threads * messages_per_thread) should be > maxLines to see rotation
                                   // e.g. 3*4 = 12 lines, maxLines = 10, so 2 lines rotate out

    LSX_LIB::Logger::Logger mt_file_logger(mt_file_config);
    threads.clear();
     for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&mt_file_logger, i, messages_per_thread]() {
            for (int j = 0; j < messages_per_thread; ++j) {
                std::string msg = "MTFile Thread " + std::to_string(i) + " message " + std::to_string(j);
                LSX_LOG_DEBUG(mt_file_logger, msg);
                 std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
        });
    }
    for (auto& t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }
    mt_file_logger.Flush();
    std::cout << "Multithreading file log test finished. Check " << mt_file_config.filepath << std::endl;
    PrintLastNLines(mt_file_config.filepath, mt_file_config.maxLines + 2);


    std::cout << "\n--- All tests completed. ---" << std::endl;
    return 0;
}
```

* 使用 `LoggerConfig` 初始化 `Logger`。
* 通过日志宏记录不同级别的日志。
* 动态更改日志级别并观察效果。
* 动态切换输出模式（控制台与文件之间）并观察效果。
* 测试文件日志的 `maxLines` 轮转行为。
* 在多线程环境中记录日志以验证线程安全性。

## 7. 注意事项与最佳实践

* **Logger 实例管理**：本模块中的 `Logger` 类本身并未实现为单例模式。您需要自行管理 `Logger` 实例的生命周期（例如，作为全局变量、静态成员，或通过依赖注入传递）。
* **性能考虑**：日志记录，特别是文件I/O和复杂的格式化，本身具有一定的性能开销。虽然此模块已考虑线程安全，但在极高并发和性能敏感的场景下，可以考虑更高级的异步日志方案（本模块未直接提供）。
* **文件路径与权限**：当使用文件输出时，请确保指定的 `filepath` 对于应用程序是可写的。如果路径无效或权限不足，`FileWriter` 会尝试向 `std::cerr` 输出错误信息。
* **错误处理**：模块内部的关键错误（如无法打开日志文件）会尝试输出到 `std::cerr`。
* **头文件包含**：通常情况下，您只需要 `#include "LSX_LIB/Logger/Logger.h"`。

---