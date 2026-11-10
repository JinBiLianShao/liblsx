/**
 * @file test_memory.cpp
 * @brief Memory 模块深度测试：Buffer / Pipe / FIFO / CircularQueue / FixedSizeQueue / SharedMemory
 */
#include "test_framework.h"

#include <atomic>
#include <chrono>
#include <cstring>
#include <fstream>
#include <thread>
#include <vector>

#include "Buffer.h"
#include "Pipe.h"
#include "FIFO.h"
#include "CircularQueue.h"
#include "CircularFixedSizeQueue.h"
#include "FixedSizeQueue.h"
#include "FixedSizePipe.h"
#include "SharedMemory.h"

using namespace LSX_LIB;
using namespace std::chrono_literals;

// ---------- Buffer ----------

LSX_TEST(Buffer, ConstructAndResize) {
    Memory::Buffer buf;
    LSX_EXPECT_EQ(buf.GetSize(), 0u);
    LSX_EXPECT_TRUE(buf.Resize(128));
    LSX_EXPECT_EQ(buf.GetSize(), 128u);
    buf.Fill(0xAB);
    uint8_t* p = buf.GetData();
    LSX_EXPECT_TRUE(p != nullptr);
    LSX_EXPECT_EQ(p[0], 0xAB);
    LSX_EXPECT_EQ(p[127], 0xAB);
}

LSX_TEST(Buffer, WriteReadAtOffsets) {
    Memory::Buffer buf(32);
    std::vector<uint8_t> data = {1, 2, 3, 4};
    LSX_EXPECT_EQ(buf.WriteAt(8, data), 4u);
    uint8_t out[4] = {};
    LSX_EXPECT_EQ(buf.ReadAt(8, out, 4), 4u);
    LSX_EXPECT_TRUE(std::memcmp(out, data.data(), 4) == 0);
    uint8_t miss[4] = {};
    LSX_EXPECT_EQ(buf.ReadAt(30, miss, 4), 2u);  // 越界部分截断
}

// ---------- Pipe ----------

LSX_TEST(Pipe, WriteThenReadRoundtrip) {
    Memory::Pipe pipe;
    std::vector<uint8_t> sent = {1, 2, 3, 4, 5};
    LSX_EXPECT_EQ(pipe.Write(sent), 5u);
    uint8_t recv[8] = {};
    size_t n = pipe.Read(recv, sizeof(recv));
    LSX_EXPECT_EQ(n, 5u);
    LSX_EXPECT_TRUE(std::memcmp(recv, sent.data(), 5) == 0);
    LSX_EXPECT_TRUE(pipe.IsEmpty());
}

LSX_TEST(Pipe, PeekDoesNotConsume) {
    Memory::Pipe pipe;
    pipe.Write(std::vector<uint8_t>{9, 8, 7});
    uint8_t peeked[3] = {};
    LSX_EXPECT_EQ(pipe.Peek(peeked, 3), 3u);
    LSX_EXPECT_TRUE(!pipe.IsEmpty());  // Peek 不消费
    uint8_t got[3] = {};
    LSX_EXPECT_EQ(pipe.Read(got, 3), 3u);
    LSX_EXPECT_EQ(got[0], 9);
}

LSX_TEST(Pipe, BlockingWriteReadAcrossThreads) {
    Memory::Pipe pipe;
    std::vector<uint8_t> payload(1024);
    for (size_t i = 0; i < payload.size(); ++i) payload[i] = static_cast<uint8_t>(i & 0xFF);
    std::thread producer([&]() {
        for (int rep = 0; rep < 4; ++rep) {
            pipe.WriteBlocking(payload, 1000);
        }
    });
    size_t total = 0;
    uint8_t chunk[512];
    while (total < payload.size() * 4) {
        size_t n = pipe.ReadBlocking(chunk, sizeof(chunk), 1000);
        if (n == 0) break;
        total += n;
    }
    producer.join();
    LSX_EXPECT_EQ(total, payload.size() * 4);
}

// ---------- FIFO ----------

LSX_TEST(FIFO, PushPopOrder) {
    Memory::FIFO<int> q;
    q.Push(1); q.Push(2); q.Push(3);
    LSX_EXPECT_EQ(q.Size(), 3u);
    auto v1 = q.Pop();
    LSX_EXPECT_TRUE(v1.has_value() && *v1 == 1);
    auto v2 = q.Pop();
    LSX_EXPECT_TRUE(v2.has_value() && *v2 == 2);
    auto v3 = q.Pop();
    auto v4 = q.Pop();  // 空队列 Pop 返回 nullopt
    LSX_EXPECT_TRUE(v3.has_value() && *v3 == 3);
    LSX_EXPECT_TRUE(!v4.has_value());
    LSX_EXPECT_TRUE(q.IsEmpty());
}

LSX_TEST(FIFO, ConcurrentProducerConsumer) {
    Memory::FIFO<int> q;
    std::atomic<int> sum{0};
    std::thread producer([&]() {
        for (int i = 1; i <= 1000; ++i) q.Push(i);
    });
    std::thread consumer([&]() {
        int got = 0;
        while (got < 1000) {
            auto v = q.Pop();
            if (v.has_value()) {
                sum += *v;
                ++got;
            } else {
                std::this_thread::yield();
            }
        }
    });
    producer.join(); consumer.join();
    LSX_EXPECT_EQ(sum.load(), 500500);  // 1..1000 求和
}

// ---------- CircularQueue ----------

LSX_TEST(CircularQueue, WrapAroundPreservesOrder) {
    Memory::CircularQueue<int> q(4);
    for (int i = 0; i < 6; ++i) q.Enqueue(i);  // 触发回绕
    std::vector<int> drained;
    while (true) {
        auto v = q.Dequeue();
        if (!v.has_value()) break;
        drained.push_back(*v);
    }
    // 具体溢出语义以实现为准：至少不能乱序或崩溃
    LSX_EXPECT_TRUE(drained.size() >= 1);
    if (drained.size() >= 2) {
        bool ordered = true;
        for (size_t i = 1; i < drained.size(); ++i)
            if (drained[i] != drained[i-1] + 1) ordered = false;
        LSX_EXPECT_TRUE(ordered);
    }
}

LSX_TEST(CircularQueue, EmptyCheckAfterDrain) {
    Memory::CircularQueue<int> q(8);
    q.Enqueue(42);
    auto v = q.Dequeue();
    LSX_EXPECT_TRUE(v.has_value());
    LSX_EXPECT_EQ(*v, 42);
    LSX_EXPECT_TRUE(!q.Dequeue().has_value());  // 空队列取不出
}

// ---------- FixedSizeQueue ----------

LSX_TEST(FixedSizeQueue, PutGetRoundtripInBlocks) {
    Memory::FixedSizeQueue q(16 /*block_size*/, 4 /*block_count*/);
    std::vector<uint8_t> block(16, 0xAB);
    LSX_EXPECT_TRUE(q.Put(block));
    uint8_t out[16];
    LSX_EXPECT_TRUE(q.Get(out, sizeof(out)));
    LSX_EXPECT_EQ(out[0], 0xAB);
    LSX_EXPECT_EQ(out[15], 0xAB);
}

LSX_TEST(FixedSizeQueue, CapacityLimitEnforced) {
    Memory::FixedSizeQueue q(16, 2);
    std::vector<uint8_t> block(16, 1);
    LSX_EXPECT_TRUE(q.Put(block));
    LSX_EXPECT_TRUE(q.Put(block));
    LSX_EXPECT_TRUE(!q.Put(block));  // 满了应拒绝（非阻塞语义）
}

LSX_TEST(FixedSizeQueue, MultiThreadTransfer) {
    Memory::FixedSizeQueue q(64, 8);
    std::atomic<int> received_bytes{0};
    std::thread producer([&]() {
        std::vector<uint8_t> data(64, 7);
        for (int i = 0; i < 50; ++i) q.PutBlocking(data, 500);
    });
    std::thread consumer([&]() {
        uint8_t buf[64];
        int got = 0;
        while (got < 50) {
            if (q.GetBlocking(buf, sizeof(buf), 500)) { received_bytes += 64; ++got; }
        }
    });
    producer.join(); consumer.join();
    LSX_EXPECT_EQ(received_bytes.load(), 50 * 64);
}

// ---------- SharedMemory ----------

LSX_TEST(SharedMemory, CreateWriteReadDestroy) {
    Memory::SharedMemory shm;
    // POSIX 实现用 ftok(key_path, 'R') 生成 key：key 必须是已存在的文件路径
    const std::string key = "/tmp/lsx_shm_key_" + std::to_string(::getpid());
    { std::ofstream touch(key); }  // 确保文件存在
    LSX_EXPECT_TRUE(shm.Create(key, 256));
    LSX_EXPECT_TRUE(shm.IsAttached());
    LSX_EXPECT_TRUE(shm.IsOwner());
    LSX_EXPECT_EQ(shm.GetSize(), 256u);
    std::vector<uint8_t> data = {0xDE, 0xAD, 0xBE, 0xEF};
    LSX_EXPECT_EQ(shm.Write(0, data), 4u);
    uint8_t back[4] = {};
    LSX_EXPECT_EQ(shm.Read(0, back, 4), 4u);
    LSX_EXPECT_TRUE(std::memcmp(back, data.data(), 4) == 0);
    shm.Detach();
    LSX_EXPECT_TRUE(!shm.IsAttached());
    LSX_EXPECT_TRUE(shm.Destroy());
}

LSX_TEST(SharedMemory, SecondProcessOrObjectOpensSameRegion) {
    Memory::SharedMemory shm1;
    const std::string key = "/tmp/lsx_shm_key2_" + std::to_string(::getpid());
    { std::ofstream touch(key); }  // ftok 需要已存在文件
    LSX_EXPECT_TRUE(shm1.Create(key, 64));
    LSX_EXPECT_EQ(shm1.Write(0, std::vector<uint8_t>{1, 2, 3}), 3u);

    Memory::SharedMemory shm2;  // 同进程内第二个对象 Open 同一区域
    LSX_EXPECT_TRUE(shm2.Open(key, 64));
    uint8_t back[3] = {};
    LSX_EXPECT_EQ(shm2.Read(0, back, 3), 3u);
    LSX_EXPECT_EQ(back[0], 1);
    LSX_EXPECT_EQ(back[2], 3);
    shm2.Detach();
    shm1.Detach();
    shm1.Destroy();
}
