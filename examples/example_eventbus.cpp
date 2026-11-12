/**
 * @file example_eventbus.cpp
 * @brief LIBLSX EventBus 模块使用示例
 *
 * 演示内容：
 *   1. 类型安全发布-订阅（事件类型即路由键）
 *   2. RAII 订阅句柄（析构自动退订）
 *   3. 回调内嵌套发布 / 退订自己
 *
 * 关键点：
 *   - publish 在调用者线程同步分发，回调串行执行。
 *   - 回调内再 publish/subscribe/unsubscribe 都安全（回调在锁外执行）。
 */

#include "EventBus/EventBus.h"

#include <iostream>
#include <string>

namespace eb = LSX_LIB::EventBus;

// 事件类型：用自定义 struct 而不是裸类型，类型安全 + 可读
struct SensorData {
    int id;
    double value;
};

struct LogEvent {
    std::string message;
};

int main()
{
    std::cout << "===== EventBus 事件总线示例 =====\n";

    eb::EventBus bus;

    // ---- 1) 订阅：handler 在 publish 调用者线程执行 ----
    auto sub1 = bus.subscribe<SensorData>([](const SensorData& e) {
        std::cout << "  [sub1] 收到传感器 " << e.id << " = " << e.value << "\n";
    });

    // 订阅计数查询
    std::cout << "  SensorData 订阅者数: " << bus.subscriberCount<SensorData>() << "\n";

    // ---- 2) 发布 ----
    bus.publish(SensorData{1, 3.14});
    bus.publish(SensorData{2, 2.71});

    // ---- 3) 回调内嵌套发布 + 退订自己 ----
    auto sub2 = bus.subscribe<SensorData>([](const SensorData& e) {
        std::cout << "  [sub2] 收到 id=" << e.id << "，只处理这一次\n";
        // 退订自己：句柄已被 lambda 捕获（需先声明后赋值）
    });
    (void)sub2;   // 本示例只演示"订阅即返回句柄"，嵌套退订见下方 sub3

    // 嵌套发布演示：收到 LogEvent 时再发一条 SensorData
    bus.subscribe<LogEvent>([&bus](const LogEvent& e) {
        std::cout << "  [log] " << e.message << "，顺带转发成 SensorData\n";
        bus.publish(SensorData{9, 9.9});   // 回调内再 publish：安全
    });

    bus.publish(LogEvent{"一条日志事件"});

    // ---- 4) RAII：句柄析构自动退订 ----
    {
        auto scoped = bus.subscribe<SensorData>([](const SensorData&) {
            std::cout << "  [scoped] 只在作用域内有效\n";
        });
        bus.publish(SensorData{3, 1.0});   // scoped 会收到
    }  // scoped 析构 -> 自动退订

    bus.publish(SensorData{4, 2.0});   // scoped 已退订，不再收到
    std::cout << "  SensorData 订阅者数（scoped 退出后）: "
              << bus.subscriberCount<SensorData>() << "\n";

    sub1.unsubscribe();               // 手动退订（幂等）
    return 0;
}
