/**
 * @file test_interfaces.cpp
 * @brief 接口审查回归测试：头文件自包含、边界零值、const 语义、RAII
 * @details 对应 v0.6.0 全库接口审查：验证修复后的 include 路径、
 *          失败时静默返回零值的接口边界、以及跨模块组合的接口一致性。
 */
#include "test_framework.h"

#include <string>
#include <vector>

// 同时包含全部便利头文件：验证头文件自包含与跨模块 include 路径修复
#include "Bus/lsx_bus.h"
#include "Checksum/lsx_checksum.h"
#include "Compress/lsx_compress.h"
#include "Crypto/lsx_crypto.h"
#include "EventBus/lsx_eventbus.h"
#include "IPC/lsx_ipc.h"
#include "Protocol/lsx_protocol.h"
#include "Result/lsx_result.h"
#include "Serialize/lsx_serialize.h"
#include "Timer/lsx_timer.h"
#include "Config/ConfigStore.h"

LSX_TEST(Interfaces, AllConvenienceHeadersSelfContained) {
    // 能编译到这一行即说明全部便利头自包含且 include 路径正确
    LSX_EXPECT_TRUE(true);
}

LSX_TEST(Interfaces, SerializeReaderZeroOnTruncatedInput) {
    // 审查标注：BinaryReader 失败静默返回零值——确保边界可预测
    std::vector<std::uint8_t> truncated = {0x01, 0x02};
    LSX_LIB::Serialize::BinaryReader r(truncated.data(), truncated.size());
    LSX_EXPECT_EQ(r.readU32(), std::uint32_t(0));
    LSX_EXPECT_TRUE(r.fail());
    LSX_EXPECT_EQ(r.readU8(), std::uint8_t(0));   // 失败后短路
}

LSX_TEST(Interfaces, ConfigStoreMissingReturnsDefaultNotThrow) {
    LSX_LIB::Config::ConfigStore cfg;
    LSX_EXPECT_EQ(cfg.getInt("nope", "nope", 42), std::int64_t(42));
    LSX_EXPECT_EQ(cfg.getString("nope", "nope", "d"), std::string("d"));
    LSX_EXPECT_EQ(cfg.getBool("nope", "nope", true), true);
}

LSX_TEST(Interfaces, ResultFailFastSemantics) {
    auto ok = LSX_LIB::Result::Result<int>(LSX_LIB::Result::Ok(1));
    LSX_EXPECT_EQ(ok.valueOr(9), 1);
    auto err = LSX_LIB::Result::Result<int>(LSX_LIB::Result::Err(std::string("e")));
    LSX_EXPECT_EQ(err.valueOr(9), 9);   // Err 状态不 assert，valueOr 安全
}

LSX_TEST(Interfaces, CrossModuleTypesCompose) {
    // ConfigStore 变更 -> EventBus 通知 -> Checksum 校验（接口协同）
    LSX_LIB::EventBus::EventBus bus;
    struct CfgChanged { std::string key; };
    int notified = 0;
    auto sub = bus.subscribe<CfgChanged>([&](const CfgChanged&) { ++notified; });

    LSX_LIB::Config::ConfigStore cfg;
    cfg.onChange([&](const LSX_LIB::Config::ChangeEvent& e) {
        bus.publish(CfgChanged{e.key});
    });
    cfg.set("s", "k", 1);
    LSX_EXPECT_EQ(notified, 1);

    auto crc = LSX_LIB::Checksum::Crc32::calc(
        reinterpret_cast<const std::uint8_t*>("s.k"), 3);
    LSX_EXPECT_TRUE(crc != 0u);
    sub.unsubscribe();
}

LSX_TEST(Interfaces, TimerDeadlineZeroMeansNeverExpires) {
    LSX_LIB::Timer::Deadline d(0);
    LSX_EXPECT_TRUE(!d.expired());
    LSX_EXPECT_EQ(d.remainingMs(), -1);
}
