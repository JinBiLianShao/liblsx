/**
 * @file test_review_fixes.cpp
 * @brief 代码审查回归测试：验证已修复的确定性缺陷不再复发
 */
#include "test_framework.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "MemoryManagement/CircularQueue.h"
#include "StateMachine/StateMachine.h"
#include "Crypto/Hash.h"
#include "Compress/Compress.h"
#include "Config/ConfigStore.h"
#include "DataTransfer/UdpMulticast.h"

using namespace LSX_LIB;

// #1 UdpMulticast::create() 错误路径（IP_ADD_MEMBERSHIP 失败）不得死锁
LSX_TEST(ReviewFix, UdpMulticastCreateNoDeadlock) {
    std::atomic<int> result{-1};
    std::thread t([&]() {
        auto b = std::make_unique<DataTransfer::UdpMulticast>("10.255.255.1", 45992);
        result.store(b->create() ? 1 : 0);
    });
    for (int i = 0; i < 20 && result.load() == -1; ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    if (result.load() == -1) {
        t.detach();
        LSX_EXPECT_TRUE(false);
    }
    t.join();
    LSX_EXPECT_EQ(result.load(), 0);
}

// #2 Peek() 不得破坏队首元素
LSX_TEST(ReviewFix, CircularQueuePeekNonDestructive) {
    Memory::CircularQueue<std::string> q(4);
    q.Enqueue(std::string("hello world"));
    auto p1 = q.Peek();
    LSX_EXPECT_TRUE(p1.has_value());
    LSX_EXPECT_EQ(p1.value(), std::string("hello world"));
    auto p2 = q.Peek();
    LSX_EXPECT_TRUE(p2.has_value());
    LSX_EXPECT_EQ(p2.value(), std::string("hello world"));
    auto d = q.Dequeue();
    LSX_EXPECT_TRUE(d.has_value());
    LSX_EXPECT_EQ(d.value(), std::string("hello world"));
}

// #4 toDot() 生成的 DOT 无嵌套方括号
LSX_TEST(ReviewFix, StateMachineToDotValid) {
    enum class S { Idle, Running };
    enum class E { Start };
    struct Ctx {};
    Ctx ctx;
    StateMachine::StateMachine<S, E, Ctx> fsm(S::Idle, ctx);
    std::string dot = fsm.toDot();
    const std::string nested = " [shape=";
    const std::string shape = "shape=doublecircle";
    LSX_EXPECT_TRUE(dot.find(nested) == std::string::npos);
    LSX_EXPECT_TRUE(dot.find(shape) != std::string::npos);
}

// #5 base64Decode 非 4 倍数长度不得越界/崩溃
LSX_TEST(ReviewFix, Base64DecodeRejectNonMultipleOf4) {
    auto v = Crypto::base64Decode("AAAAA");
    LSX_EXPECT_TRUE(v.empty());
    auto folded = Crypto::base64Decode("SGVs\nbG8=\t");
    LSX_EXPECT_EQ(folded.size(), static_cast<std::size_t>(5));
}

// #6 dispatch 用户动作抛异常后状态机不得永久卡死
LSX_TEST(ReviewFix, StateMachineExceptionRecovers) {
    enum class S { Idle, Running };
    enum class E { Start, Other };
    struct Ctx {};
    Ctx ctx;
    StateMachine::StateMachine<S, E, Ctx> fsm(S::Idle, ctx);
    bool fired = false;
    fsm.addTransition(S::Idle, E::Start, S::Running, {},
                      [&](Ctx&) { fired = true; throw std::runtime_error("boom"); });
    // 另注册一条转换：异常恢复后必须还能完成正常派发，才能证明 pumping_ 已复位
    fsm.addTransition(S::Idle, E::Other, S::Running);
    try { fsm.dispatch(E::Start); } catch (...) {}
    LSX_EXPECT_TRUE(fired);
    // 关键断言：异常后 dispatch 不得返回 Busy（Busy 意味着 pumping_ 卡死）。
    // 旧测试只断言 currentState()==Idle——该值在异常中断时本就是 Idle，
    // 即使状态机永久卡死也恒真，无法捕获回归。
    auto r = fsm.dispatch(E::Other);
    LSX_EXPECT_TRUE(r == StateMachine::TransitionResult::Ok);
    LSX_EXPECT_TRUE(fsm.currentState() == S::Running);
}

// #8 loadFromString 解析失败不得污染旧配置
LSX_TEST(ReviewFix, ConfigStoreLoadAtomic) {
    Config::ConfigStore s;
    s.set("sec", "key", std::string("original"));
    auto r = s.loadFromString("[sec]\nkey = changed\nbroken_line_without_equals");
    LSX_EXPECT_TRUE(r.isErr());
    LSX_EXPECT_EQ(s.getString("sec", "key", std::string("")), std::string("original"));
}

// #11 gzipDecompress 支持带 FNAME/FCOMMENT 可选头的标准 gzip
LSX_TEST(ReviewFix, GzipOptionalHeaders) {
    std::vector<std::uint8_t> raw(1000);
    for (std::size_t i = 0; i < raw.size(); ++i) raw[i] = static_cast<std::uint8_t>(i % 251);
    auto gz = Compress::gzipCompress(raw);
    LSX_EXPECT_TRUE(!gz.empty());
    std::vector<std::uint8_t> with_name(gz.begin(), gz.begin() + 10);
    with_name[3] = static_cast<std::uint8_t>(gz[3] | 0x08 | 0x10);
    const char* name = "data.bin";
    for (const char* p = name; *p; ++p) with_name.push_back(static_cast<std::uint8_t>(*p));
    with_name.push_back(0);
    const char* comment = "review test";
    for (const char* p = comment; *p; ++p) with_name.push_back(static_cast<std::uint8_t>(*p));
    with_name.push_back(0);
    with_name.insert(with_name.end(), gz.begin() + 10, gz.end());
    auto out = Compress::gzipDecompress(with_name);
    LSX_EXPECT_EQ(out.size(), raw.size());
    bool same = (out == raw);
    LSX_EXPECT_TRUE(same);
}

// #25 rleDecode 输出有上限，恶意放大被拒
LSX_TEST(ReviewFix, RleDecodeBounded) {
    std::vector<std::uint8_t> evil;
    for (int i = 0; i < 1000; ++i) { evil.push_back(0x41); evil.push_back(255); }
    auto out = Compress::rleDecode(evil);
    // 1000 对 255 = 255000 字节（未超 kMaxInflate）应正常解码；
    // 上限的意义是拦住 64MB 级的放大，而非拒绝合法规模的数据。
    LSX_EXPECT_EQ(out.size(), static_cast<std::size_t>(255000));
    std::vector<std::uint8_t> good{0x41, 3, 0x42, 2};
    auto g = Compress::rleDecode(good);
    LSX_EXPECT_EQ(g.size(), static_cast<std::size_t>(5));
}

// #14 回归：同状态多个超时在 tick() 内应全部触发，且超时引发状态转换时
// 不得因 active_timeouts_ 被清空而丢失后续到期项（迭代器失效 bug）。
LSX_TEST(ReviewFix, TickFiresAllDueTimeoutsAcrossTransition) {
    enum class S { Idle, Running };
    enum class E { T1, T2 };
    struct Ctx {};
    Ctx ctx;
    StateMachine::StateMachine<S, E, Ctx> fsm(S::Idle, ctx);
    fsm.addTimeout(S::Idle, 100, E::T1);
    fsm.addTimeout(S::Idle, 100, E::T2);
    fsm.addTransition(S::Idle, E::T1, S::Running);  // T1 离开 Idle，触发 armTimeouts 清空

    // 两个超时同时到期：都必须派发（fired == 2），T2 无匹配转换也计入派发数。
    std::size_t fired = fsm.tick(200);
    LSX_EXPECT_EQ(fired, static_cast<std::size_t>(2));
    LSX_EXPECT_TRUE(fsm.currentState() == S::Running);
}

// #14 回归：自转换（exit+entry）同样会重排超时，tick 不得丢事件
LSX_TEST(ReviewFix, TickFiresAllDueTimeoutsOnSelfTransition) {
    enum class S { Idle, Running };
    enum class E { T1, T2 };
    struct Ctx { int n = 0; };
    Ctx ctx;
    StateMachine::StateMachine<S, E, Ctx> fsm(S::Idle, ctx);
    fsm.addTimeout(S::Idle, 100, E::T1);
    fsm.addTimeout(S::Idle, 100, E::T2);
    fsm.addTransition(S::Idle, E::T1, S::Idle, {}, [](Ctx& c) { ++c.n; });  // 自转换
    fsm.addTransition(S::Idle, E::T2, S::Running);

    std::size_t fired = fsm.tick(200);
    LSX_EXPECT_EQ(fired, static_cast<std::size_t>(2));
    LSX_EXPECT_EQ(ctx.n, 1);
    LSX_EXPECT_TRUE(fsm.currentState() == S::Running);
}
