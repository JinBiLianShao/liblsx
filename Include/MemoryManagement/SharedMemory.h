/**
 * @file SharedMemory.h
 * @brief 进程间共享内存管理类
 * @details 定义了 LIB_LSX::Memory 命名空间下的 SharedMemory 类，
 * 用于实现跨平台的进程间共享内存的管理。
 * 该类封装了 Windows 和 POSIX (Linux/macOS) 系统下共享内存操作的底层细节，
 * 提供统一的接口进行共享内存段的创建、打开、附加（映射）、分离（解除映射）和销毁。
 * 支持在附加的共享内存段上进行线程安全的读写操作。
 * 类禁用了拷贝和赋值，因为管理 OS 资源的所有权和生命周期在拷贝时非常复杂。
 * @author 连思鑫（liansixin）
 * @date 2025-5-7
 * @version 1.0
 *
 * ### 核心功能
 * - **跨平台支持**: 封装了 Windows 和 POSIX 系统下的共享内存 API。
 * - **创建/打开**: 支持创建新的共享内存段或附加到已存在的段。
 * - **附加/分离**: 将共享内存段映射到当前进程的地址空间，或从地址空间解除映射。
 * - **销毁**: 支持标记共享内存段进行销毁（通常由创建者完成）。
 * - **读写操作**: 提供在附加的共享内存段上进行数据读写的方法。
 * - **线程安全**: 使用内部互斥锁 (`std::mutex`) 保护对共享内存段的并发读写访问。
 * - **状态查询**: 提供 `GetSize`, `IsAttached`, `IsOwner` 方法查询共享内存段的状态。
 * - **资源管理**: RAII 模式，析构函数处理资源的清理（分离和可能的销毁）。
 *
 * ### 使用示例
 *
 * @code
 * #include "SharedMemory.h"
 * #include <iostream>
 * #include <vector>
 * #include <thread>
 * #include <chrono>
 * #include <cstring> // For memcpy, strcmp
 *
 * // 示例：进程 A (创建者)
 * // 编译并运行此代码，它将创建一个共享内存段，写入数据，然后等待
 * // 另一个进程 B 附加并读取。
 * void process_a() {
 * const std::string shm_name = "MySharedMemorySegment";
 * const size_t shm_size = 1024;
 *
 * LIB_LSX::Memory::SharedMemory shm_a;
 *
 * std::cout << "[Process A] Attempting to create shared memory: " << shm_name << " with size " << shm_size << std::endl;
 * if (shm_a.Create(shm_name, shm_size)) {
 * std::cout << "[Process A] Shared memory created and attached. Size: " << shm_a.GetSize() << std::endl;
 *
 * // 写入数据
 * const char* message = "Hello from Process A!";
 * std::vector<uint8_t> data_to_write(message, message + strlen(message) + 1); // +1 for null terminator
 * size_t written = shm_a.Write(0, data_to_write);
 * if (written > 0) {
 * std::cout << "[Process A] Wrote " << written << " bytes to shared memory." << std::endl;
 * } else {
 * std::cerr << "[Process A] Failed to write to shared memory." << std::endl;
 * }
 *
 * std::cout << "[Process A] Waiting for Process B to read (sleeping)..." << std::endl;
 * std::this_thread::sleep_for(std::chrono::seconds(5)); // 等待另一个进程附加和读取
 *
 * // 在退出前，由创建者销毁共享内存段
 * std::cout << "[Process A] Destroying shared memory segment." << std::endl;
 * if (shm_a.Destroy()) {
 * std::cout << "[Process A] Shared memory segment marked for destruction." << std::endl;
 * } else {
 * std::cerr << "[Process A] Failed to destroy shared memory segment." << std::endl;
 * }
 *
 * } else {
 * std::cerr << "[Process A] Failed to create shared memory." << std::endl;
 * }
 *
 * // SharedMemory 对象析构时会自动 Detach
 * std::cout << "[Process A] Exiting." << std::endl;
 * }
 *
 * // 示例：进程 B (附加者)
 * // 编译并运行此代码，它将尝试附加到由进程 A 创建的共享内存段，读取数据，然后分离。
 * // 确保在进程 A 运行并创建共享内存后运行此代码。
 * void process_b() {
 * const std::string shm_name = "MySharedMemorySegment";
 * const size_t shm_size = 1024; // 附加时也需要知道大小 (至少是创建时的大小)
 *
 * LIB_LSX::Memory::SharedMemory shm_b;
 *
 * std::cout << "[Process B] Attempting to open shared memory: " << shm_name << " with size " << shm_size << std::endl;
 * // 尝试附加，可能需要等待进程 A 创建
 * int retry_count = 5;
 * bool attached = false;
 * while (retry_count-- > 0) {
 * if (shm_b.Open(shm_name, shm_size)) {
 * attached = true;
 * std::cout << "[Process B] Successfully attached to shared memory." << std::endl;
 * break;
 * }
 * std::cerr << "[Process B] Failed to open shared memory, retrying..." << std::endl;
 * std::this_thread::sleep_for(std::chrono::seconds(1));
 * }
 *
 * if (attached) {
 * // 读取数据
 * std::vector<uint8_t> read_buffer(shm_size);
 * size_t read_bytes = shm_b.Read(0, read_buffer.data(), read_buffer.size());
 * if (read_bytes > 0) {
 * // 找到 null 终止符以打印字符串
 * size_t message_len = 0;
 * while (message_len < read_bytes && read_buffer[message_len] != '\0') {
 * message_len++;
 * }
 * std::string received_message(read_buffer.begin(), read_buffer.begin() + message_len);
 * std::cout << "[Process B] Read " << read_bytes << " bytes from shared memory. Message: \"" << received_message << "\"" << std::endl;
 * } else {
 * std::cerr << "[Process B] Failed to read from shared memory or read 0 bytes." << std::endl;
 * }
 *
 * // Process B 只需 Detach，不销毁 (销毁由创建者 Process A 完成)
 * std::cout << "[Process B] Detaching from shared memory." << std::endl;
 * shm_b.Detach();
 *
 * } else {
 * std::cerr << "[Process B] Failed to attach to shared memory after multiple retries." << std::endl;
 * }
 *
 * std::cout << "[Process B] Exiting." << std::endl;
 * }
 *
 * int main(int argc, char* argv[]) {
 * // 简单的命令行参数判断来模拟两个进程
 * if (argc > 1 && strcmp(argv[1], "producer") == 0) {
 * process_a(); // 运行生产者逻辑
 * } else if (argc > 1 && strcmp(argv[1], "consumer") == 0) {
 * process_b(); // 运行消费者逻辑
 * } else {
 * std::cout << "Usage: " << argv[0] << " [producer|consumer]" << std::endl;
 * }
 *
 * return 0;
 * }
 * @endcode
 *
 * ### 注意事项
 * - **平台依赖**: 底层实现完全依赖于 Windows API (CreateFileMapping, MapViewOfFile) 或 POSIX API (shmget, shmat, shmdt, shmctl)。
 * - **生命周期**: 共享内存段的生命周期独立于创建它的进程。通常，创建者负责在不再需要时销毁它。销毁操作只是标记段进行删除，实际删除发生在所有附加的进程都分离之后。
 * - **同步**: SharedMemory 类内部的互斥锁只保证对 *单个 SharedMemory 对象* 的线程安全访问。**不同进程之间访问同一个共享内存段时，需要额外的进程间同步机制**（如进程间互斥锁、信号量等），此 SharedMemory 类本身不提供进程间同步。
 * - **错误处理**: OS 级别的共享内存操作可能会失败（例如，权限问题、资源不足、段不存在）。方法通常返回 false 或 nullptr 来指示失败。
 * - **Key/Name**: 在 POSIX 系统上通常使用 `ftok` 生成 key，或者使用 `/dev/shm` 下的文件名。在 Windows 上使用字符串名称。`key_or_name` 参数需要根据平台进行适当处理。此接口抽象了这部分，但具体实现需要处理平台差异。
 * - **大小**: 附加到现有段时，通常需要知道段的大小。传递的大小参数应与创建时的大小匹配或小于它。
 * - **拷贝/移动**: 类禁用了拷贝和赋值，因为管理 OS 资源的所有权和生命周期在拷贝时非常复杂且容易出错。移动语义通常是可能的，但需要仔细实现以安全转移 OS 句柄/标识符和所有权标志。
 */

#ifndef LIB_LSX_MEMORY_SHARED_MEMORY_H
#define LIB_LSX_MEMORY_SHARED_MEMORY_H

#include <cstddef> // For size_t
#include <string> // For std::string
#include <vector> // For convenience read/write methods
#include <mutex> // For thread safety (std::mutex, std::lock_guard)

// Include OS-specific headers only in the .cpp for implementation details
// #ifdef _WIN32
// #include <windows.h> // For HANDLE, LPVOID etc.
// #else // POSIX
// #include <sys/ipc.h> // For IPC_CREAT, IPC_EXCL, IPC_RMID, IPC_STAT
// #include <sys/shm.h> // For shmget, shmat, shmdt, shmctl
// #include <sys/types.h> // For key_t
// #include <unistd.h> // For ftok
// #endif


/**
 * @brief LSX 库的根命名空间。
 */
namespace LIB_LSX {
    /**
     * @brief 内存管理相关的命名空间。
     * 包含内存缓冲区和相关工具。
     */
    namespace Memory {

        /**
         * @brief 进程间共享内存管理类。
         * 实现跨平台的进程间共享内存的管理，允许不同进程访问同一块物理内存。
         * 提供创建、打开、附加、分离和销毁共享内存段的功能，并支持线程安全的读写访问。
         */
        // 5. SharedMemory Module (共享内存模块)
        // 实现进程间共享内存的管理 (非模板类)
        // 接口优化：增加更多状态检查，便捷的读写方法
        // 线程安全：使用 std::mutex
        class SharedMemory {
        private:
            // OS-specific identifiers and handles - opaque in header
            // 使用 void* 来避免在头文件中包含 Windows.h，保持跨平台兼容性
#ifdef _WIN32
            /**
             * @brief Windows 文件映射对象句柄。
             * 在 Windows 系统下，用于标识共享内存段。
             * 初始化为 nullptr。
             */
            void* hMapFile_ = nullptr; // Use void* to avoid including windows.h in header
            /**
             * @brief Windows 共享内存段在当前进程地址空间中的基地址。
             * 初始化为 nullptr。
             */
            void* lpBaseAddress_ = nullptr;
#else // POSIX
            /**
             * @brief POSIX 共享内存段标识符。
             * 在 POSIX 系统下，用于标识共享内存段。
             * 初始化为 -1。
             */
            int shm_id_ = -1;
            /**
             * @brief POSIX 共享内存段在当前进程地址空间中的地址。
             * 初始化为 nullptr。
             */
            void* shm_address_ = nullptr;
            /**
             * @brief POSIX 共享内存段的 System V IPC 键。
             * 通常通过 ftok 生成，用于在不同进程中引用同一个共享内存段。
             * 初始化为 -1。
             */
            key_t shm_key_ = -1; // Use -1 for invalid key initially
#endif
            /**
             * @brief 共享内存段的大小（字节）。
             * 在创建或打开时确定。
             */
            size_t size_ = 0;
            /**
             * @brief 标志，指示此 SharedMemory 实例是否是共享内存段的创建者。
             * 只有创建者通常应该调用 Destroy 方法。
             */
            bool is_owner_ = false; // True if this instance created the segment

            /**
             * @brief 共享内存段的键/名称表示。
             * 用于在不同进程中识别同一个共享内存段。在 POSIX 上可能是 ftok 生成的 key，在 Windows 上是字符串名称。
             */
            std::string key_name_; // Conceptual key/name representation
            /**
             * @brief 互斥锁。
             * 用于保护对共享内存段的并发读写访问，确保线程安全。
             * **重要提示：** 此锁仅在 *同一个进程内* 有效。不同进程之间访问同一个共享内存段时，需要额外的进程间同步机制。
             */
            mutable std::mutex mutex_; // Thread safety mutex

        public:
            // Constructor/Destructor
            /**
             * @brief 默认构造函数。
             * 初始化 SharedMemory 对象，不创建或附加到任何共享内存段。
             */
            SharedMemory();
            /**
             * @brief 析构函数。
             * 在对象销毁时自动调用 Detach()，如果此实例是创建者，还可能尝试调用 Destroy()。
             * 确保释放与共享内存相关的 OS 资源。
             */
            ~SharedMemory(); // Destructor handles cleanup

            // Prevent copying and assignment as managing OS resources is complex
            /**
             * @brief 禁用拷贝构造函数。
             * 共享内存句柄和所有权标志是 OS 资源，拷贝不安全。
             */
            SharedMemory(const SharedMemory&) = delete;
            /**
             * @brief 禁用拷贝赋值运算符。
             * 共享内存句柄和所有权标志是 OS 资源，拷贝不安全。
             */
            SharedMemory& operator=(const SharedMemory&) = delete;
            // Consider adding move constructor/assignment if managing resources allows safe transfer

            // --- Management Functions ---
            /**
             * @brief 创建并附加到一个新的共享内存段。
             * 根据指定的键/名称和大小创建一个新的共享内存段，并将其附加到当前进程的地址空间。
             * 如果已存在同名的共享内存段，此操作可能会失败（取决于 OS API 的标志）。
             *
             * @param key_or_name 用于标识共享内存段的键或名称（例如，POSIX 上的文件路径用于 ftok，或 Windows 上的字符串名称）。
             * @param size 共享内存段的大小（字节）。必须大于 0。
             * @return 如果成功创建并附加，返回 true；如果失败（例如，已存在同名段，权限问题，内存不足），返回 false。
             */
            // Create and attach to a shared memory segment
            // key_or_name: identifier for the shared memory (e.g., file path for ftok, or a name string)
            // size: size of the segment in bytes
            // Returns true on success, false on failure
            bool Create(const std::string& key_or_name, size_t size);

            /**
             * @brief 附加到一个已存在的共享内存段。
             * 根据指定的键/名称和大小附加到（映射）一个已存在的共享内存段。
             *
             * @param key_or_name 用于标识共享内存段的键或名称。
             * @param size 共享内存段的大小（字节）。通常应与创建时的大小匹配。
             * @return 如果成功附加，返回 true；如果失败（例如，段不存在，权限问题，大小不匹配），返回 false。
             */
            // Attach to an existing shared memory segment
            // Returns true on success, false on failure
            bool Open(const std::string& key_or_name, size_t size);

            /**
             * @brief 从进程的地址空间分离共享内存段。
             * 解除共享内存段在当前进程地址空间中的映射。分离后，此 SharedMemory 对象将不再有效，不能进行读写操作。
             * 多次调用是安全的。
             */
            // Detach the shared memory segment from the process's address space
            void Detach();

            /**
             * @brief 销毁共享内存段。
             * 标记共享内存段进行删除。实际删除将在所有附加到该段的进程都调用 Detach 后发生。
             * 通常只有创建者应该调用此方法。
             *
             * @return 如果成功发起销毁（或段已被标记），返回 true；如果失败（例如，权限问题，段不存在），返回 false。
             */
            // Destroy the shared memory segment (only the owner should typically do this)
            // The segment is marked for deletion and will be removed when all processes detach
            // Returns true if successfully initiated destruction (or already marked), false on failure
            bool Destroy();

            // --- Data Access Functions ---
            /**
             * @brief 获取附加的共享内存段的基地址。
             * 返回共享内存段在当前进程地址空间中的起始地址。
             * **注意：** 获取地址后，用户需要自行保证对数据的访问是线程安全的，特别是在多进程和多线程环境中。
             *
             * @return 共享内存段的基地址。如果未附加，返回 nullptr。
             */
            // Get the base address of the attached shared memory segment
            void* GetAddress() const;

            /**
             * @brief 在共享内存段的指定偏移量写入数据。
             * 将指定数据从给定的偏移量开始写入附加的共享内存段。
             * 此操作是线程安全的（在同一个进程内），通过内部互斥锁保护。
             *
             * @param offset 写入的起始偏移量（从 0 开始）。
             * @param data 指向要写入数据的缓冲区。
             * @param size 要写入数据的字节数。
             * @return 实际写入的字节数。如果成功写入且在段范围内，返回 size；如果 offset 或 offset + size 超出段范围，返回 0。
             */
            // Write data to the shared memory segment at a given offset
            // Returns number of bytes written (equal to size if successful within bounds, 0 otherwise)
            size_t Write(size_t offset, const uint8_t* data, size_t size);
            /**
             * @brief 在共享内存段的指定偏移量写入数据。
             * 便利方法，将 std::vector 中的数据从给定的偏移量开始写入附加的共享内存段。
             * 此操作是线程安全的（在同一个进程内）。
             *
             * @param offset 写入的起始偏移量（从 0 开始）。
             * @param data 包含要写入数据的 std::vector。
             * @return 实际写入的字节数。如果成功写入且在段范围内，返回 data.size()；如果 offset 或 offset + data.size() 超出段范围，返回 0。
             */
            size_t Write(size_t offset, const std::vector<uint8_t>& data);

            /**
             * @brief 从共享内存段的指定偏移量读取数据。
             * 从附加的共享内存段中指定偏移量开始读取数据到给定的缓冲区。
             * 此操作是线程安全的（在同一个进程内），通过内部互斥锁保护。
             *
             * @param offset 读取的起始偏移量（从 0 开始）。
             * @param buffer 指向用于存储读取数据的缓冲区。
             * @param size 要读取的最大字节数。
             * @return 实际读取的字节数。如果成功读取且在段范围内，返回 size；如果 offset 或 offset + size 超出段范围，返回 0。
             */
            // Read data from the shared memory segment at a given offset
            // Returns number of bytes read (equal to size if successful within bounds, 0 otherwise)
            size_t Read(size_t offset, uint8_t* buffer, size_t size) const;
            /**
             * @brief 从共享内存段的指定偏移量读取数据。
             * 便利方法，从附加的共享内存段中指定偏移量开始读取指定数量的数据，并返回一个新的 std::vector。
             * 此操作是线程安全的（在同一个进程内）。
             *
             * @param offset 读取的起始偏移量（从 0 开始）。
             * @param size 要读取的字节数。
             * @return 包含读取数据的 std::vector。如果 offset 或 offset + size 超出段范围，返回一个空的 vector。
             */
            std::vector<uint8_t> Read(size_t offset, size_t size) const;


            // --- Status Functions ---
            /**
             * @brief 获取共享内存段的大小。
             *
             * @return 共享内存段的大小（字节）。如果未附加，返回 0。
             */
            // Get the size of the shared memory segment
            size_t GetSize() const;

            /**
             * @brief 检查共享内存段是否当前附加到此进程。
             *
             * @return 如果共享内存段已成功附加，返回 true；否则返回 false。
             */
            // Check if the shared memory segment is currently attached to this process
            bool IsAttached() const;

            /**
             * @brief 检查此 SharedMemory 实例是否是共享内存段的创建者。
             *
             * @return 如果此实例创建了共享内存段，返回 true；否则返回 false。
             */
            // Check if this instance was the creator of the shared memory segment
            bool IsOwner() const;

            // Check if a shared memory segment with the given key/name exists (platform dependent)
            // This is a conceptual interface, implementation requires OS-specific lookups.
            // static bool Exists(const std::string& key_or_name); // 需要具体实现
        };

    } // namespace Memory
} // namespace LIB_LSX

#endif // LIB_LSX_MEMORY_SHARED_MEMORY_H
