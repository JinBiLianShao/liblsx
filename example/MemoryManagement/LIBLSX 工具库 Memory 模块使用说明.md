好的，下面是一份详细的使用说明文档，介绍了如何使用您设计的 C++ 内存管理工具库 (`LIB_LSX::Memory`)。

---

# LSX 内存管理工具库 (`LIB_LSX::Memory`) 使用说明

## 概述

`LIB_LSX::Memory` 是一个 C++ 工具库，提供了一系列用于内存管理和数据结构实现的模块。这些模块包括先进先出 (FIFO) 队列、管道 (Pipe)、循环队列 (CircularQueue)、通用队列 (Queue)、共享内存 (SharedMemory) 以及内存缓冲区 (Buffer)。为了满足现代多线程应用程序的需求，所有模块都设计为线程安全的，并提供了一致的接口和管理功能。

库的主要特点：

* **模块化设计:** 每个功能都由一个独立的类实现。
* **面向对象:** 封装了数据和操作。
* **命名空间:** 所有组件都位于 `LIB_LSX::Memory` 命名空间下。
* **线程安全:** 通过内部使用 `std::mutex` 和 `std::condition_variable` 保证在多线程环境下的安全访问。
* **灵活的队列/管道:** 提供处理任意数据类型 (`template<T>`) 的队列和处理字节流的管道。
* **固定大小块管理:** 提供专门用于高效管理固定大小内存块的队列和管道模块。
* **共享内存:** 提供了跨进程共享内存的基本管理功能。
* **内存缓冲区:** 提供了动态大小的内存块管理功能。

## 命名空间

库中的所有类和函数都位于 `LIB_LSX::Memory` 命名空间下。在使用时，您可以通过以下方式引用：

```cpp
LIB_LSX::Memory::Buffer myBuffer;
```

或者，在函数或源文件顶部使用 `using namespace` 简化：

```cpp
using namespace LIB_LSX::Memory;
Buffer myBuffer;
```

## 通用接口约定

为了保持一致性，许多模块采用了类似的接口命名约定：

* **管理函数:** `Clear()`, `Resize()`, `Create()`, `Open()`, `Detach()`, `Destroy()`, `Fill()`.
* **数据存取函数:** `Put()` (写入/入队), `Get()` (读取/出队), `Peek()` (查看但不移除), `WriteAt()`, `ReadAt()`, `Write()`, `Read()`.
    * 队列类常用 `Put`/`Get` 作为 `Push`/`Pop` 的别名或主名称。
    * 管道类常用 `Write`/`Read` 作为数据流存取的主名称。
    * 缓冲区和共享内存使用 `WriteAt`/`ReadAt` 或 `Write`/`Read` 进行带偏移量的直接内存存取。
* **状态函数:** `Is...()` (例如 `IsEmpty()`, `IsFull()`, `IsAttached()`, `IsOwner()`), `Size()`, `Capacity()`, `BlockSize()`, `BlockCount()`, `TotalSize()`, `GetAddress()`, `GetData()`.

某些方法（如 `Get()`, `Pop()`, `Read()`）在操作失败（如队列空、管道空）时可能返回 `std::optional` 或 `bool` 并通过参数拷贝数据，这取决于具体的模块设计。需要抛出异常的情况（如 `Peek()` 空队列，构造函数参数无效）会使用标准 C++ 异常。

## 线程安全说明

库中的每个模块都设计为线程安全的。这意味着您可以从多个线程并发地调用同一个对象的公共方法，而无需额外的外部锁定。

* **内部同步:** 每个类内部都有一个 `mutable std::mutex mutex_` 成员，用于保护其共享的可变状态（如队列的头尾指针、大小、底层数据结构）。所有公共方法在访问或修改这些状态前都会锁定此互斥量，并在方法结束时自动解锁（通过 `std::lock_guard` 或 `std::unique_lock`）。
* **条件变量:** 对于支持阻塞操作的模块（如 `FixedSizeQueue`, `CircularFixedSizeQueue`, `FixedSizePipe`, `Pipe` 的阻塞读写），使用了 `std::condition_variable` 来实现线程间的等待和通知机制。
* **SharedMemory 注意事项:** `SharedMemory` 类保证了对共享内存段 *句柄和地址* 的获取以及通过其 `Write`/`Read` 方法进行的数据复制是线程安全的。但是，如果您获取 SharedMemory 的裸指针 (`GetAddress()`) 并在多个线程中直接读写该内存区域 *内部* 的复杂数据结构（例如，您在共享内存中实现了一个自定义链表），那么您需要自己负责在该共享内存区域内实现额外的同步机制（例如，在共享内存段中放置互斥量或使用原子操作），以保护该数据结构的完整性。`SharedMemory` 类本身不提供这种内部数据结构的同步。

## 模块详细说明

### 1. FIFO 模块 (`FIFO<T>`)

实现一个通用的先进先出 (FIFO) 队列，可以存储任意类型 `T` 的数据。底层使用 `std::queue`。

* **用途:** 需要按顺序处理元素的通用场景。
* **特点:** 无界（容量受系统内存限制），线程安全。

**类定义:**

```cpp
template<typename T>
class FIFO { ... };
```

**构造函数:**

```cpp
FIFO(); // 创建一个空的 FIFO 队列
```

**管理函数:**

* `void Clear();` : 清空队列中的所有元素。

**数据存取函数:**

* `void Push(const T& value);` : 将元素添加到队列尾部。
* `void Put(const T& value);` : `Push` 的别名。
* `std::optional<T> Pop();` : 移除并返回队列头部的元素。如果队列为空，返回 `std::nullopt`。
* `std::optional<T> Get();` : `Pop` 的别名。
* `const T& Peek() const;` : 返回队列头部的元素，但不移除。如果队列为空，抛出 `std::out_of_range` 异常。

**状态函数:**

* `bool IsEmpty() const;` : 检查队列是否为空。
* `size_t Size() const;` : 返回队列中元素的数量。

**示例:**

```cpp
FIFO<std::string> string_fifo;
string_fifo.Put("Hello");
string_fifo.Put("World");
if (auto str = string_fifo.Get()) {
    std::cout << "FIFO: " << *str << std::endl; // 输出 Hello
}
std::cout << "FIFO size: " << string_fifo.Size() << std::endl; // 输出 1
string_fifo.Clear();
```

### 2. Pipe 模块 (`Pipe`)

实现一个通用的字节流管道，适用于传输可变长度的字节数据。底层使用 `std::deque<uint8_t>`。

* **用途:** 处理字节流，如网络通信、文件读写缓冲等。
* **特点:** 线程安全，支持非阻塞和阻塞读写。默认是无界（容量受系统内存限制）。

**类定义:**

```cpp
class Pipe { ... };
```

**构造函数:**

```cpp
Pipe(); // 创建一个空的管道
```

**管理函数:**

* `void Clear();` : 清空管道中的所有字节。

**数据存取函数 (非阻塞):**

* `size_t Write(const uint8_t* data, size_t size);` : 写入指定数量的字节到管道。返回实际写入的字节数（对于当前无界实现，通常等于 `size`）。
* `size_t Write(const std::vector<uint8_t>& data);` : 写入 `std::vector<uint8_t>` 到管道。
* `size_t Put(const uint8_t* data, size_t size);` : `Write` 的别名。
* `size_t Put(const std::vector<uint8_t>& data);` : `Write` 的别名。
* `size_t Read(uint8_t* buffer, size_t size);` : 从管道读取最多指定数量的字节到缓冲区。返回实际读取的字节数（可能小于 `size`，如果管道数据不足或为空）。
* `std::vector<uint8_t> Read(size_t size);` : 从管道读取最多指定数量的字节，返回 `std::vector<uint8_t>`。
* `size_t Get(uint8_t* buffer, size_t size);` : `Read` 的别名。
* `std::vector<uint8_t> Get(size_t size);` : `Read` 的别名。
* `size_t Peek(uint8_t* buffer, size_t size) const;` : 查看（但不移除）管道头部的字节。返回实际查看的字节数。
* `std::vector<uint8_t> Peek(size_t size) const;` : 查看管道头部的字节，返回 `std::vector<uint8_t>`。

**数据存取函数 (阻塞):**

* `size_t ReadBlocking(uint8_t* buffer, size_t size, long timeout_ms = -1);` : 阻塞读取字节。
    * `timeout_ms < 0`: 无限等待直到有数据可读。
    * `timeout_ms == 0`: 非阻塞（行为同 `Read`）。
    * `timeout_ms > 0`: 最多等待指定毫秒。
    * 返回实际读取的字节数，超时返回 0。
* `std::vector<uint8_t> ReadBlocking(size_t size, long timeout_ms = -1);` : 阻塞读取字节，返回 `std::vector<uint8_t>`。
* `size_t WriteBlocking(const uint8_t* data, size_t size, long timeout_ms = -1);` : 阻塞写入字节。对于无界管道，行为同 `Write`。对于潜在的有界管道实现，可能会等待直到有足够空间。返回实际写入字节数，超时返回 0。
* `size_t WriteBlocking(const std::vector<uint8_t>& data, long timeout_ms = -1);` : 阻塞写入字节，接收 `std::vector<uint8_t>`。

**状态函数:**

* `bool IsEmpty() const;` : 检查管道是否为空。
* `size_t Size() const;` : 返回管道中当前字节的数量。

**示例:**

```cpp
Pipe byte_pipe;
std::vector<uint8_t> send = { 'a', 'b', 'c' };
byte_pipe.Write(send);
std::vector<uint8_t> received = byte_pipe.Read(5); // Reads max 5 bytes
std::cout << "Pipe read " << received.size() << " bytes." << std::endl;
std::vector<uint8_t> peeked = byte_pipe.Peek(1);
std::cout << "Pipe peeked " << peeked.size() << " byte." << std::endl;
```

### 3. CircularQueue 模块 (`CircularQueue<T>`)

实现一个通用的循环队列，可以存储任意类型 `T` 的数据。它具有固定的容量，并循环利用底层存储空间。

* **用途:** 需要固定内存占用、高效存取元素的缓冲场景。
* **特点:** 有界，固定容量，线程安全。

**类定义:**

```cpp
template<typename T>
class CircularQueue { ... };
```

**构造函数:**

```cpp
CircularQueue(size_t usable_capacity); // 创建指定可用容量的循环队列。usable_capacity >= 1。
```

**管理函数:**

* `void Clear();` : 清空队列（逻辑上重置指针，不擦除数据）。

**数据存取函数:**

* `bool Enqueue(const T& value);` : 将元素添加到队列尾部。如果队列已满，返回 `false`。
* `bool Put(const T& value);` : `Enqueue` 的别名。
* `std::optional<T> Dequeue();` : 移除并返回队列头部的元素。如果队列为空，返回 `std::nullopt`。
* `std::optional<T> Get();` : `Dequeue` 的别名。
* `const T& Peek() const;` : 返回队列头部的元素，但不移除。如果队列为空，抛出 `std::out_of_range` 异常。

**状态函数:**

* `bool IsEmpty() const;` : 检查队列是否为空。
* `bool IsFull() const;` : 检查队列是否已满。
* `size_t Size() const;` : 返回队列中当前元素的数量。
* `size_t Capacity() const;` : 返回队列的最大可用容量。

**示例:**

```cpp
CircularQueue<int> int_cqueue(5); // 容量为 5
int_cqueue.Put(1);
int_cqueue.Put(2);
if (int_cqueue.Put(3)) std::cout << "Enqueued 3" << std::endl;
std::cout << "CQueue size: " << int_cqueue.Size() << ", full: " << int_cqueue.IsFull() << std::endl;
if (auto val = int_cqueue.Get()) {
    std::cout << "Dequeued: " << *val << std::endl; // 输出 1
}
```

### 4. Queue 模块 (`Queue<T>`)

实现一个通用的队列功能，类似于 `FIFO`，但保留了 `std::deque` 的通用命名。

* **用途:** 与 `FIFO` 类似，通用队列处理场景。
* **特点:** 无界（容量受系统内存限制），线程安全。

**类定义:**

```cpp
template<typename T>
class Queue { ... };
```

**构造函数:**

```cpp
Queue(); // 创建一个空的队列
```

**管理函数:**

* `void Clear();` : 清空队列。

**数据存取函数:**

* `void Push(const T& value);` : 将元素添加到队列尾部。
* `void Put(const T& value);` : `Push` 的别名。
* `std::optional<T> Pop();` : 移除并返回队列头部的元素。如果队列为空，返回 `std::nullopt`。
* `std::optional<T> Get();` : `Pop` 的别名。
* `const T& Peek() const;` : 返回队列头部的元素，但不移除。如果队列为空，抛出 `std::out_of_range` 异常。

**状态函数:**

* `bool IsEmpty() const;` : 检查队列是否为空。
* `size_t Size() const;` : 返回队列中元素的数量。

**示例:**

```cpp
Queue<double> double_queue;
double_queue.Put(3.14);
double_queue.Put(2.71);
if (auto val = double_queue.Get()) {
    std::cout << "Queue: " << *val << std::endl; // 输出 3.14
}
```

### 5. SharedMemory 模块 (`SharedMemory`)

实现进程间共享内存的基本管理功能。

* **用途:** 在同一台机器上的不同进程之间高效地共享数据。
* **特点:** OS 依赖，线程安全（针对对象的管理操作和直接读写方法）。

**类定义:**

```cpp
class SharedMemory { ... };
```

**构造函数:**

```cpp
SharedMemory(); // 创建一个 SharedMemory 对象实例
```

**管理函数:**

* `bool Create(const std::string& key_or_name, size_t size);` : 创建并附加到一个新的共享内存段。如果已存在同名段，通常会失败（取决于 OS 实现和标志）。`key_or_name` 用于标识共享内存段（在 POSIX 系统上通常是用于 `ftok` 的文件路径，在 Windows 上是对象名称）。`size` 为段的大小（字节）。成功返回 `true`。
* `bool Open(const std::string& key_or_name, size_t size);` : 附加到已存在的共享内存段。`size` 用于指定映射视图的大小（通常等于或小于实际段大小）。成功返回 `true`。
* `void Detach();` : 从当前进程的地址空间分离共享内存段。分离后，对象将不再关联到该段。
* `bool Destroy();` : 标记共享内存段进行销毁。通常只有创建者才应该调用此方法。段会在最后一个进程分离后被 OS 实际删除。成功返回 `true`。

**数据存取函数:**

* `void* GetAddress() const;` : 获取附加的共享内存段在当前进程中的起始地址。如果未附加，返回 `nullptr`。注意：直接使用此指针进行多线程/多进程读写需要用户自行实现同步。
* `size_t Write(size_t offset, const uint8_t* data, size_t size);` : 在指定偏移量向共享内存段写入数据。进行了边界检查。返回实际写入字节数。
* `size_t Write(size_t offset, const std::vector<uint8_t>& data);` : 在指定偏移量向共享内存段写入 `std::vector<uint8_t>`。
* `size_t Read(size_t offset, uint8_t* buffer, size_t size) const;` : 从指定偏移量读取数据到缓冲区。进行了边界检查。返回实际读取字节数。
* `std::vector<uint8_t> Read(size_t offset, size_t size) const;` : 从指定偏移量读取数据，返回 `std::vector<uint8_t>`。

**状态函数:**

* `size_t GetSize() const;` : 返回 SharedMemory 对象关联的段大小（通常是 `Create` 或 `Open` 时指定的）。
* `bool IsAttached() const;` : 检查 SharedMemory 对象是否已成功附加到 OS 共享内存段。
* `bool IsOwner() const;` : 检查 SharedMemory 对象实例是否是该共享内存段的创建者。
* `static bool Exists(const std::string& key_or_name);` : （概念性接口，未实现）检查一个共享内存段是否存在。

**示例:**

```cpp
// Process 1 (Creator)
SharedMemory shm_creator;
if (shm_creator.Create("/tmp/my_shm_file", 1024)) { // Use a file path for POSIX ftok
    if (shm_creator.IsOwner()) {
        uint8_t* addr = static_cast<uint8_t*>(shm_creator.GetAddress());
        if (addr) {
            shm_creator.Write(0, (const uint8_t*)"Hello SHM", 9);
        }
        shm_creator.Detach();
        // In a real app, Process 1 might wait here, then call shm_creator.Destroy();
    }
}

// Process 2 (Reader)
SharedMemory shm_reader;
if (shm_reader.Open("/tmp/my_shm_file", 1024)) {
     std::vector<uint8_t> data = shm_reader.Read(0, 10); // Read 10 bytes
     std::cout << "Read from SHM: " << std::string(data.begin(), data.end()) << std::endl;
     shm_reader.Detach();
}
```

### 6. Buffer 模块 (`Buffer`)

实现一个可动态调整大小的内存缓冲区。

* **用途:** 通用内存数据处理、临时存储。
* **特点:** 可变大小，线程安全。

**类定义:**

```cpp
class Buffer { ... };
```

**构造函数:**

```cpp
Buffer();       // 创建一个空缓冲区
Buffer(size_t size); // 创建指定初始大小的缓冲区
```

**管理函数:**

* `bool Resize(size_t new_size);` : 调整缓冲区大小。如果新大小小于当前大小，数据会被截断；如果大于，新区域未初始化。返回 `true` 成功，`false` 失败（如内存不足）。
* `void Clear();` : 清空缓冲区（等同于 `Resize(0)`）。
* `void Fill(uint8_t value);` : 用指定字节值填充整个缓冲区。

**数据存取函数:**

* `uint8_t* GetData();` : 获取指向缓冲区底层数据的指针。注意：直接使用此指针进行多线程读写需要用户自行实现同步，或者确保在指针使用期间没有 `Resize` 或 `Clear` 操作发生。推荐使用 `WriteAt`/`ReadAt`。
* `const uint8_t* GetData() const;` : 获取常量指针。
* `size_t WriteAt(size_t offset, const uint8_t* data, size_t size);` : 在指定偏移量向缓冲区写入数据。返回实际写入字节数。
* `size_t WriteAt(size_t offset, const std::vector<uint8_t>& data);` : 在指定偏移量向缓冲区写入 `std::vector<uint8_t>`。
* `size_t ReadAt(size_t offset, uint8_t* buffer, size_t size) const;` : 从指定偏移量读取数据到缓冲区。返回实际读取字节数。
* `std::vector<uint8_t> ReadAt(size_t offset, size_t size) const;` : 从指定偏移量读取数据，返回 `std::vector<uint8_t>`。

**状态函数:**

* `size_t GetSize() const;` : 返回缓冲区当前的大小（字节数）。
* `bool IsEmpty() const;` : 检查缓冲区是否为空。
* `size_t Capacity() const;` : 返回缓冲区的容量（等同于当前大小）。

**示例:**

```cpp
Buffer myBuffer(10); // 10 bytes
myBuffer.Fill(0xAB);
uint8_t data_to_write[] = { 0x11, 0x22, 0x33 };
myBuffer.WriteAt(2, data_to_write, 3); // Write at offset 2
std::vector<uint8_t> read_data = myBuffer.ReadAt(0, 5); // Read 5 bytes from offset 0
// Buffer content: AB AB 11 22 33 AB AB AB AB AB
// read_data content: AB AB 11 22 33
myBuffer.Resize(20);
std::cout << "Buffer size: " << myBuffer.GetSize() << std::endl; // 输出 20
```

### 7. FixedSizeQueue 模块 (`FixedSizeQueue`)

实现一个存储固定大小内存块的有界 FIFO 队列。

* **用途:** 高效地在生产者-消费者模式中传输固定大小的消息或数据块。
* **特点:** 有界（容量由块数量决定），线程安全，支持阻塞和非阻塞存取。

**类定义:**

```cpp
class FixedSizeQueue { ... };
```

**构造函数:**

```cpp
FixedSizeQueue(size_t block_size, size_t block_count); // 创建指定块大小和块数量的队列
```

**管理函数:**

* `void Clear();` : 清空队列。

**数据存取函数 (非阻塞):**

* `bool Put(const uint8_t* data, size_t data_size);` : 将一个数据块放入队列尾部。`data_size` 必须等于 `BlockSize()`。如果队列满或大小不匹配，返回 `false`。
* `bool Put(const std::vector<uint8_t>& data);` : 放入 `std::vector<uint8_t>` 块。
* `bool Get(uint8_t* buffer, size_t buffer_size);` : 从队列头部获取一个数据块。`buffer_size` 必须至少等于 `BlockSize()`。如果队列空或缓冲区太小，返回 `false`。
* `std::optional<std::vector<uint8_t>> Get();` : 获取一个数据块，返回 `std::vector<uint8_t>`。
* `bool Peek(uint8_t* buffer, size_t buffer_size) const;` : 查看队列头部的块（不移除）。行为类似非阻塞 Get。
* `std::optional<std::vector<uint8_t>> Peek() const;` : 查看队列头部的块，返回 `std::vector<uint8_t>`。

**数据存取函数 (阻塞):**

* `bool GetBlocking(uint8_t* buffer, size_t buffer_size, long timeout_ms = -1);` : 阻塞获取块，等待直到队列非空。超时机制同 `Pipe::ReadBlocking`。成功返回 `true`，超时返回 `false`。
* `std::optional<std::vector<uint8_t>> GetBlocking(long timeout_ms = -1);` : 阻塞获取块，返回 `std::optional<std::vector<uint8_t>>`。
* `bool PutBlocking(const uint8_t* data, size_t data_size, long timeout_ms = -1);` : 阻塞放入块，等待直到队列非满。超时机制同 `Pipe::WriteBlocking`。成功返回 `true`，超时返回 `false`。
* `bool PutBlocking(const std::vector<uint8_t>& data, long timeout_ms = -1);` : 阻塞放入块，接收 `std::vector<uint8_t>`。

**状态函数:**

* `bool IsEmpty() const;` : 检查队列是否为空。
* `bool IsFull() const;` : 检查队列是否已满。
* `size_t Size() const;` : 返回队列中当前块的数量。
* `size_t BlockSize() const;` : 返回每个块的大小（字节）。
* `size_t BlockCount() const;` : 返回队列的最大块数量。
* `size_t TotalSize() const;` : 返回队列总的内存占用（`BlockSize() * BlockCount()`）。

**示例:**

```cpp
FixedSizeQueue fsq(16, 100); // 16 bytes per block, 100 blocks capacity
std::vector<uint8_t> my_block(16, 0xAA);
if (fsq.Put(my_block)) {
    std::cout << "Put block successfully." << std::endl;
}
std::vector<uint8_t> received_block(16);
if (fsq.Get(received_block.data(), received_block.size())) {
    std::cout << "Got block successfully." << std::endl;
}

// Example with blocking
std::thread writer([&]{ fsq.PutBlocking(my_block, -1); }); // Infinite wait Put
std::thread reader([&]{ fsq.GetBlocking(received_block.data(), received_block.size(), -1); }); // Infinite wait Get
writer.join();
reader.join();
```

### 8. CircularFixedSizeQueue 模块 (`CircularFixedSizeQueue`)

实现一个存储固定大小内存块的有界循环队列。行为与 `FixedSizeQueue` 类似，但 Put 操作在队列满时会覆盖最旧的块。

* **用途:** 需要固定内存占用、固定大小块、且新数据可以覆盖旧数据的缓冲场景（如日志缓冲、音频/视频帧缓冲）。
* **特点:** 有界，固定容量，线程安全，支持阻塞和非阻塞存取，满时覆盖。

**类定义:**

```cpp
class CircularFixedSizeQueue { ... };
```

**构造函数:**

```cpp
CircularFixedSizeQueue(size_t block_size, size_t block_count); // 创建指定块大小和块数量的循环队列
```

**管理函数:**

* `void Clear();` : 清空队列。

**数据存取函数 (非阻塞):**

* `bool Put(const uint8_t* data, size_t data_size);` : 将一个数据块放入队列尾部。`data_size` 必须等于 `BlockSize()`。如果队列满，会覆盖最旧的块，返回 `true`。大小不匹配返回 `false`。
* `bool Put(const std::vector<uint8_t>& data);` : 放入 `std::vector<uint8_t>` 块。
* `bool Get(uint8_t* buffer, size_t buffer_size);` : 从队列头部获取一个数据块。`buffer_size` 必须至少等于 `BlockSize()`。如果队列空或缓冲区太小，返回 `false`。
* `std::optional<std::vector<uint8_t>> Get();` : 获取一个数据块，返回 `std::vector<uint8_t>`。
* `bool Peek(uint8_t* buffer, size_t buffer_size) const;` : 查看队列头部的块（不移除）。行为类似非阻塞 Get。
* `std::optional<std::vector<uint8_8t>> Peek() const;` : 查看队列头部的块，返回 `std::vector<uint8_t>`。

**数据存取函数 (阻塞):**

* `bool GetBlocking(uint8_t* buffer, size_t buffer_size, long timeout_ms = -1);` : 阻塞获取块，等待直到队列非空。超时机制同 `Pipe::ReadBlocking`。成功返回 `true`，超时返回 `false`。
* `std::optional<std::vector<uint8_t>> GetBlocking(long timeout_ms = -1);` : 阻塞获取块，返回 `std::optional<std::vector<uint8_t>>`。
* `bool PutBlocking(const uint8_t* data, size_t data_size, long timeout_ms = -1);` : 阻塞放入块，等待直到队列非满。由于循环队列满时会覆盖，此方法通常只在 `timeout_ms > 0` 且队列暂时满时等待，无限等待 (`-1`) 或非阻塞 (`0`) 行为通常是立即覆盖或失败（如果大小不匹配）。此处实现为等待直到有空间 *或者* 队列已满并等待超时后进行覆盖。成功返回 `true`，超时返回 `false`。
* `bool PutBlocking(const std::vector<uint8_t>& data, long timeout_ms = -1);` : 阻塞放入块，接收 `std::vector<uint8_t>`。

**状态函数:**

* `bool IsEmpty() const;` : 检查队列是否为空。
* `bool IsFull() const;` : 检查队列是否已满。
* `size_t Size() const;` : 返回队列中当前块的数量。
* `size_t BlockSize() const;` : 返回每个块的大小（字节）。
* `size_t BlockCount() const;` : 返回队列的最大块数量。
* `size_t TotalSize() const;` : 返回队列总的内存占用（`BlockSize() * BlockCount()`）。

**示例:**

```cpp
CircularFixedSizeQueue cfsq(8, 5); // 8 bytes per block, 5 blocks capacity
std::vector<uint8_t> block_a(8, 0xA);
std::vector<uint8_t> block_b(8, 0xB);
for(int i=0; i<5; ++i) cfsq.Put(block_a); // Fill queue
cfsq.Put(block_b); // Overwrites the first block_a
if(auto b = cfsq.Get()) { // Should get a block_a (the second one put)
    // ... check content ...
}
if(auto b = cfsq.Get()) { // Should get block_b
    // ... check content ...
}
```

### 9. FixedSizePipe 模块 (`FixedSizePipe`)

实现一个传输固定大小内存块的管道。功能和行为类似于 `FixedSizeQueue`，但命名更贴近 Pipe 的读写概念。

* **用途:** 高效地在生产者-消费者模式中传输固定大小的消息或数据块，强调数据流的概念。
* **特点:** 有界（容量由块数量决定），线程安全，支持阻塞和非阻塞读写。

**类定义:**

```cpp
class FixedSizePipe { ... };
```

**构造函数:**

```cpp
FixedSizePipe(size_t block_size, size_t block_count); // 创建指定块大小和块数量的管道
```

**管理函数:**

* `void Clear();` : 清空管道。

**数据存取函数 (非阻塞):**

* `bool Write(const uint8_t* data, size_t data_size);` : 写入一个数据块到管道。`data_size` 必须等于 `BlockSize()`。如果管道满或大小不匹配，返回 `false`。
* `bool Write(const std::vector<uint8_t>& data);` : 写入 `std::vector<uint8_t>` 块。
* `bool Read(uint8_t* buffer, size_t buffer_size);` : 从管道读取一个数据块。`buffer_size` 必须至少等于 `BlockSize()`。如果管道空或缓冲区太小，返回 `false`。
* `std::optional<std::vector<uint8_t>> Read();` : 读取一个数据块，返回 `std::vector<uint8_t>`。
* `bool Peek(uint8_t* buffer, size_t buffer_size) const;` : 查看管道头部的块（不移除）。行为类似非阻塞 Read。
* `std::optional<std::vector<uint8_t>> Peek() const;` : 查看管道头部的块，返回 `std::vector<uint8_t>`。

**数据存取函数 (阻塞):**

* `bool ReadBlocking(uint8_t* buffer, size_t buffer_size, long timeout_ms = -1);` : 阻塞读取块，等待直到管道非空。超时机制同 `Pipe::ReadBlocking`。成功返回 `true`，超时返回 `false`。
* `std::optional<std::vector<uint8_8t>> ReadBlocking(long timeout_ms = -1);` : 阻塞读取块，返回 `std::optional<std::vector<uint8_t>>`。
* `bool WriteBlocking(const uint8_t* data, size_t data_size, long timeout_ms = -1);` : 阻塞写入块，等待直到管道非满。超时机制同 `Pipe::WriteBlocking`。成功返回 `true`，超时返回 `false`。
* `bool WriteBlocking(const std::vector<uint8_8t>& data, long timeout_ms = -1);` : 阻塞写入块，接收 `std::vector<uint8_t>`。

**状态函数:**

* `bool IsEmpty() const;` : 检查管道是否为空。
* `bool IsFull() const;` : 检查管道是否已满。
* `size_t Size() const;` : 返回管道中当前块的数量。
* `size_t BlockSize() const;` : 返回每个块的大小（字节）。
* `size_t BlockCount() const;` : 返回管道的最大块数量。
* `size_t TotalSize() const;` : 返回管道总的内存占用（`BlockSize() * BlockCount()`）。

**示例:**

```cpp
FixedSizePipe fsp(32, 50); // 32 bytes per block, 50 blocks capacity
std::vector<uint8_t> message(32); // Populate message...
if (fsp.Write(message)) {
    std::cout << "Wrote message block." << std::endl;
}
std::vector<uint8_t> received_message(32);
if (fsp.ReadBlocking(received_message.data(), received_message.size(), 500)) { // Wait up to 500ms
    std::cout << "Read message block (blocking)." << std::endl;
} else {
    std::cerr << "Failed to read message block (timeout)." << std::endl;
}
```
---