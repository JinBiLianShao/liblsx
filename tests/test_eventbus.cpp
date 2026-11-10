/**
 * @file test_eventbus.cpp
 * @brief EventBus 模块深度测试：订阅/发布/退订/多类型/嵌套发布/句柄语义
 */
#include "test_framework.h"

#include <atomic>
#include <string>
#include <thread>
#include <vector>

#include "EventBus/EventBus.h"

using namespace LSX_LIB;

namespace
{
struct EvClick { int x; int y; };
struct EvKey { int code; };
struct EvData { std::string payload; };
}

LSX_TEST(EventBus, SubscribeAndPublish) {
    EventBus::EventBus bus;
    std::atomic<int> got_x{-1};
    auto sub = bus.subscribe<EvClick>([&](const EvClick& e) { got_x = e.x; });
    LSX_EXPECT_TRUE(sub.isValid());
    LSX_EXPECT_EQ(bus.subscriberCount<EvClick>(), 1u);
    bus.publish(EvClick{42, 0});
    LSX_EXPECT_EQ(got_x.load(), 42);
}

LSX_TEST(EventBus, MultipleSubscribersAllNotified) {
    EventBus::EventBus bus;
    std::atomic<int> a{0}, b{0};
    auto s1 = bus.subscribe<EvKey>([&](const EvKey&) { ++a; });
    auto s2 = bus.subscribe<EvKey>([&](const EvKey&) { ++b; });
    bus.publish(EvKey{7});
    LSX_EXPECT_EQ(a.load(), 1);
    LSX_EXPECT_EQ(b.load(), 1);
    s1.unsubscribe();
    bus.publish(EvKey{8});
    LSX_EXPECT_EQ(a.load(), 1);   // 已退订不再收
    LSX_EXPECT_EQ(b.load(), 2);   // 仍订阅继续收
}

LSX_TEST(EventBus, TypesAreIsolated) {
    EventBus::EventBus bus;
    std::atomic<int> clicks{0}, keys{0};
    auto s1 = bus.subscribe<EvClick>([&](const EvClick&) { ++clicks; });
    auto s2 = bus.subscribe<EvKey>([&](const EvKey&) { ++keys; });
    bus.publish(EvClick{1, 2});
    bus.publish(EvClick{3, 4});
    bus.publish(EvKey{5});
    LSX_EXPECT_EQ(clicks.load(), 2);
    LSX_EXPECT_EQ(keys.load(), 1);
}

LSX_TEST(EventBus, UnsubscribeStopsDelivery) {
    EventBus::EventBus bus;
    std::atomic<int> n{0};
    {
        auto sub = bus.subscribe<EvData>([&](const EvData&) { ++n; });
        bus.publish(EvData{"a"});
        // sub 在此析构 → 自动退订
    }
    bus.publish(EvData{"b"});
    LSX_EXPECT_EQ(n.load(), 1);  // 只收到析构前的那条
    LSX_EXPECT_EQ(bus.subscriberCount<EvData>(), 0u);
}

LSX_TEST(EventBus, PublishWithNoSubscriberIsNoop) {
    EventBus::EventBus bus;
    bus.publish(EvClick{1, 1});  // 无订阅者，不崩溃
    LSX_EXPECT_EQ(bus.subscriberCount<EvClick>(), 0u);
}

LSX_TEST(EventBus, NestedPublishInsideHandler) {
    EventBus::EventBus bus;
    std::atomic<int> keys_seen{0};
    // 处理 EvClick 时再发布 EvKey
    auto s1 = bus.subscribe<EvClick>([&](const EvClick&) {
        bus.publish(EvKey{1});
    });
    auto s2 = bus.subscribe<EvKey>([&](const EvKey&) { ++keys_seen; });
    bus.publish(EvClick{0, 0});
    LSX_EXPECT_EQ(keys_seen.load(), 1);  // 锁外分发支持嵌套发布
}

LSX_TEST(EventBus, SubscribeInsideHandler) {
    EventBus::EventBus bus;
    std::atomic<bool> new_sub_called{false};
    auto s1 = bus.subscribe<EvClick>([&](const EvClick& e) {
        if (e.x == 0)
        {
            auto s2 = bus.subscribe<EvKey>([&](const EvKey&) { new_sub_called = true; });
            // s2 立即析构退订——只验证回调内 subscribe 不死锁
        }
    });
    bus.publish(EvClick{0, 0});
    bus.publish(EvKey{9});
    LSX_EXPECT_TRUE(true);
}

LSX_TEST(EventBus, UnsubscribeInsideHandler) {
    EventBus::EventBus bus;
    std::atomic<int> called{0};
    // 先声明句柄再订阅（回调需要引用句柄自身退订）
    EventBus::Subscription sub;
    sub = bus.subscribe<EvClick>([&](const EvClick&) {
        ++called;
        sub.unsubscribe();  // 回调里退订自己
    });
    bus.publish(EvClick{1, 1});
    bus.publish(EvClick{2, 2});
    LSX_EXPECT_EQ(called.load(), 1);  // 退订后第二次不再收
}

LSX_TEST(EventBus, CopiedSubscriptionSingleUnsubscribe) {
    EventBus::EventBus bus;
    std::atomic<int> n{0};
    auto sub = bus.subscribe<EvData>([&](const EvData&) { ++n; });
    EventBus::Subscription copy = sub;  // 句柄拷贝共享状态
    copy.unsubscribe();
    sub.unsubscribe();  // 幂等：第二份退订不应出错
    bus.publish(EvData{"x"});
    LSX_EXPECT_EQ(n.load(), 0);
}

LSX_TEST(EventBus, MovedSubscriptionStillWorks) {
    EventBus::EventBus bus;
    std::atomic<int> n{0};
    EventBus::Subscription sub;
    {
        auto tmp = bus.subscribe<EvData>([&](const EvData&) { ++n; });
        sub = std::move(tmp);
    }
    bus.publish(EvData{"m"});
    LSX_EXPECT_EQ(n.load(), 1);
    LSX_EXPECT_TRUE(sub.isValid());
}

LSX_TEST(EventBus, ConcurrentPublishSerialize) {
    EventBus::EventBus bus;
    std::atomic<int> total{0};
    auto sub = bus.subscribe<EvData>([&](const EvData& e) {
        total += static_cast<int>(e.payload.size());
    });
    std::vector<std::thread> publishers;
    for (int t = 0; t < 4; ++t)
    {
        publishers.emplace_back([&bus]() {
            for (int i = 0; i < 100; ++i)
            {
                bus.publish(EvData{"ab"});  // 每次 +2
            }
        });
    }
    for (auto& th : publishers) th.join();
    LSX_EXPECT_EQ(total.load(), 800);  // 4*100*2
}

LSX_TEST(EventBus, BusDestroyInvalidatesSubscriptions) {
    auto bus = std::make_unique<EventBus::EventBus>();
    std::atomic<int> n{0};
    auto sub = bus->subscribe<EvData>([&](const EvData&) { ++n; });
    LSX_EXPECT_TRUE(sub.isValid());
    bus.reset();  // 总线销毁
    LSX_EXPECT_TRUE(!sub.isValid());
    sub.unsubscribe();  // 对已销毁总线退订不崩溃
    LSX_EXPECT_TRUE(true);
}
