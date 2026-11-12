/**
 * @file example_memory.cpp
 * @brief LIBLSX Memory 模块使用示例
 *
 * 演示内容：
 *   1. Buffer          —— 动态字节缓冲区（偏移读写、填充、尺寸管理）
 *   2. Pipe            —— 字节流管道（阻塞/非阻塞读写）
 *   3. CircularQueue   —— 固定容量循环队列（模板，非阻塞）
 *   4. FIFO            —— 无界先进先出队列（模板）
 *   5. SharedMemory    —— 进程间共享内存（POSIX）
 *
 * 关键点：
 *   - 所有容器内部自带互斥锁，多线程可安全共享；拷贝构造被禁用（含锁状态）。
 *   - CircularQueue 容量为「可用元素数」，满时 Enqueue 返回 false，空时 Dequeue 返回 nullopt。
 *   - SharedMemory 的 Create 是「独占创建」（已存在则失败），Open 是「附加已存在」。
 */

#include "MemoryManagement/Buffer.h"
#include "MemoryManagement/Pipe.h"
#include "MemoryManagement/CircularQueue.h"
#include "MemoryManagement/FIFO.h"
#include "MemoryManagement/SharedMemory.h"

#include <cstring>
#include <iostream>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace mem = LSX_LIB::Memory;

// ---------------------------------------------------------------------------
// 演示 1：Buffer 动态字节缓冲区
// ---------------------------------------------------------------------------
static void demo_buffer()
{
    std::cout << "\n===== 演示 1：Buffer 字节缓冲区 =====\n";

    mem::Buffer buf(16);                  // 初始 16 字节
    buf.Fill(0xAA);                       // 全部填 0xAA
    auto first = buf.ReadAt(0, 1);        // 读偏移 0 的 1 字节
    std::cout << "  初始大小 " << buf.GetSize() << "，首字节 0x"
              << std::hex << static_cast<int>(first.empty() ? 0 : first[0])
              << std::dec << "\n";

    // 在偏移 4 处写入 3 字节
    std::vector<std::uint8_t> payload{0x01, 0x02, 0x03};
    buf.WriteAt(4, payload);

    // 读回偏移 4 的 3 字节
    auto got = buf.ReadAt(4, 3);
    std::cout << "  偏移 4 读出 " << got.size() << " 字节: 0x";
    for (auto b : got) std::cout << std::hex << static_cast<int>(b) << " ";
    std::cout << std::dec << "\n";

    buf.Resize(8);                        // 缩到 8 字节（尾部截断）
    buf.Clear();                          // 清空（大小归 0）
    std::cout << "  清空后大小 " << buf.GetSize() << "\n";
}

// ---------------------------------------------------------------------------
// 演示 2：Pipe 字节流管道
// ---------------------------------------------------------------------------
static void demo_pipe()
{
    std::cout << "\n===== 演示 2：Pipe 字节流管道 =====\n";

    mem::Pipe pipe;

    // 非阻塞写：无界管道总是全部写入
    std::vector<std::uint8_t> data{
        static_cast<std::uint8_t>('H'), static_cast<std::uint8_t>('e'),
        static_cast<std::uint8_t>('l'), static_cast<std::uint8_t>('l'),
        static_cast<std::uint8_t>('o')};
    pipe.Write(data);
    std::cout << "  写入 " << data.size() << " 字节，管道当前 " << pipe.Size() << " 字节\n";

    // 非阻塞读：读走 3 字节，剩 2 字节
    auto part = pipe.Read(3);
    std::cout << "  读走 " << part.size() << " 字节，剩余 " << pipe.Size() << " 字节\n";

    // 阻塞读（带 200ms 超时）：管道还有数据，立即返回
    auto rest = pipe.ReadBlocking(64, 200);
    std::cout << "  阻塞读得 " << rest.size() << " 字节\n";

    // 再阻塞读：管道已空，200ms 后超时返回空
    auto empty = pipe.ReadBlocking(64, 200);
    std::cout << "  空管道阻塞读：超时返回 " << empty.size() << " 字节\n";
}

// ---------------------------------------------------------------------------
// 演示 3：CircularQueue 固定容量循环队列
// ---------------------------------------------------------------------------
static void demo_circular_queue()
{
    std::cout << "\n===== 演示 3：CircularQueue 循环队列 =====\n";

    mem::CircularQueue<std::string> q(3);  // 可用容量 3

    q.Enqueue(std::string("alpha"));
    q.Enqueue(std::string("beta"));
    q.Enqueue(std::string("gamma"));
    bool full_rejected = !q.Enqueue(std::string("delta"));  // 满则失败
    std::cout << "  第 4 个入队被拒：" << (full_rejected ? "是" : "否") << "\n";

    // Peek 查看队首但不移除（返回 optional<T>，不破坏元素）
    if (auto front = q.Peek()) {
        std::cout << "  队首（Peek）: " << *front << "\n";
    }

    // Dequeue 逐个取出（空则 nullopt）
    while (auto v = q.Dequeue()) {
        std::cout << "  取出: " << *v << "\n";
    }
    std::cout << "  队列已空，Dequeue 返回 "
              << (q.Dequeue().has_value() ? "有值" : "nullopt") << "\n";
}

// ---------------------------------------------------------------------------
// 演示 4：FIFO 无界先进先出队列
// ---------------------------------------------------------------------------
static void demo_fifo()
{
    std::cout << "\n===== 演示 4：FIFO 无界队列 =====\n";

    mem::FIFO<int> fifo;
    fifo.Put(1);
    fifo.Put(2);
    fifo.Put(3);

    // Get 空时返回 nullopt
    while (auto v = fifo.Get()) {
        std::cout << "  取出: " << *v << "\n";
    }
}

// ---------------------------------------------------------------------------
// 演示 5：SharedMemory 进程间共享内存（POSIX）
// ---------------------------------------------------------------------------
static void demo_shared_memory()
{
    std::cout << "\n===== 演示 5：SharedMemory 共享内存 =====\n";

    mem::SharedMemory shm;

    // Create：独占创建（若已存在则失败）。key 需指向一个存在的文件路径，
    // 库内部用 ftok(path, 'R') 生成 System V key。
    const std::string path = "/tmp/lsx_shm_example";
    if (shm.Create(path, 256)) {
        std::cout << "  创建共享内存段成功，大小 " << shm.GetSize() << " 字节\n";

        // 写入数据
        std::vector<std::uint8_t> msg{static_cast<std::uint8_t>('H'), static_cast<std::uint8_t>('i')};
        shm.Write(0, msg);

        // 读回
        auto back = shm.Read(0, 2);
        std::cout << "  读回 " << back.size() << " 字节\n";

        // 创建者负责销毁（析构也会 Detach）
        shm.Destroy();
        std::cout << "  已销毁共享内存段\n";
    } else {
        std::cout << "  创建失败（可能已存在或权限不足）\n";
    }
}

int main()
{
    demo_buffer();
    demo_pipe();
    demo_circular_queue();
    demo_fifo();
    demo_shared_memory();
    return 0;
}
