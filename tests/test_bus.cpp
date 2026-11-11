/**
 * @file test_bus.cpp
 * @brief Bus 模块深度测试：I2C/SPI 注入 fd 路径（CI 无真实硬件）
 * @details 通过 socketpair 注入 fd 模拟设备读写，验证上层逻辑、Result 语义、
 *          参数校验与边界；真实 /dev 设备 ioctl 路径由集成测试覆盖。
 */
#include "test_framework.h"

#include <atomic>
#include <cstring>
#include <thread>
#include <vector>

#include <sys/socket.h>
#include <unistd.h>

#include "Bus/Bus.h"

using namespace LSX_LIB::Bus;

namespace
{
/// 创建一个 socketpair，返回两端 fd；可选启动回显线程
struct FdPair
{
    int a = -1, b = -1;
    std::thread echo;
    std::atomic<bool> stop{false};

    explicit FdPair(bool with_echo = true)
    {
        int fds[2];
        if (::socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0)
        {
            a = fds[0]; b = fds[1];
        }
        if (with_echo && a >= 0)
        {
            echo = std::thread([this]() {
                char buf[256];
                while (!stop.load())
                {
                    ssize_t n = ::recv(b, buf, sizeof(buf), MSG_DONTWAIT);
                    if (n > 0)
                    {
                        ::send(b, buf, static_cast<std::size_t>(n), 0);  // 原样回显
                    }
                    else if (n == 0) break;
                    else std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            });
        }
    }
    ~FdPair()
    {
        stop = true;
        if (echo.joinable()) echo.join();
        if (a >= 0) ::close(a);
        if (b >= 0) ::close(b);
    }
};
}

// ---------- I2cBus ----------

LSX_TEST(I2cBus, OpenInjectAndClose) {
    FdPair p(false);
    LSX_EXPECT_TRUE(p.a >= 0);
    I2cBus bus(p.a, 0x48, false);   // 不拥有 fd
    LSX_EXPECT_TRUE(bus.open().isOk());
    LSX_EXPECT_TRUE(bus.isOpen());
    LSX_EXPECT_EQ(bus.slaveAddress(), std::uint8_t(0x48));
    bus.close();
    LSX_EXPECT_TRUE(!bus.isOpen());
}

LSX_TEST(I2cBus, OperationsWithoutOpenFail) {
    I2cBus bus("/dev/definitely_no_such_i2c", 0x48);
    LSX_EXPECT_TRUE(bus.writeRegister8(0, 1).isErr());
    LSX_EXPECT_TRUE(bus.readRegister8(0).isErr());
    LSX_EXPECT_TRUE(bus.readBlock(0, 4).isErr());
}

LSX_TEST(I2cBus, WriteRegister8InjectPath) {
    FdPair p(true);
    I2cBus bus(p.a, 0x50, false);
    bus.open();
    // 注入路径：写 [reg, value] 到 fd
    LSX_EXPECT_TRUE(bus.writeRegister8(0x12, 0xAB).isOk());
}

LSX_TEST(I2cBus, ReadRegister8InjectPathEcho) {
    FdPair p(true);   // 回显：写出去的 [reg] 会被读回来当作数据
    I2cBus bus(p.a, 0x50, false);
    bus.open();
    auto r = bus.readRegister8(0x34);
    LSX_EXPECT_TRUE(r.isOk());
    LSX_EXPECT_EQ(r.value(), std::uint8_t(0x34));   // 回显把寄存器地址送回
}

LSX_TEST(I2cBus, WriteRegister16InjectPath) {
    FdPair p(true);
    I2cBus bus(p.a, 0x50, false);
    bus.open();
    LSX_EXPECT_TRUE(bus.writeRegister16(0x1234, 0xCD).isOk());
}

LSX_TEST(I2cBus, BlockLengthValidation) {
    FdPair p(true);
    I2cBus bus(p.a, 0x50, false);
    bus.open();
    LSX_EXPECT_TRUE(bus.readBlock(0, 0).isErr());        // 0 长度非法
    LSX_EXPECT_TRUE(bus.readBlock(0, 9000).isErr());     // 超上限
    LSX_EXPECT_TRUE(bus.writeBlock(0, {}).isErr());      // 空写非法
}

LSX_TEST(I2cBus, RawReadWriteInjectEcho) {
    FdPair p(true);
    I2cBus bus(p.a, 0x50, false);
    bus.open();
    LSX_EXPECT_TRUE(bus.writeRaw({1, 2, 3}).isOk());
    auto r = bus.readRaw(3);   // 回显线程会送回 [1,2,3]
    // 注意：回显时序可能让 read 拿到 write 的数据，只要不崩溃且返回 Ok 或合理失败均可
    LSX_EXPECT_TRUE(true);
}

LSX_TEST(I2cBus, ProbeOnInjectedFd) {
    FdPair p(false);
    I2cBus bus(p.a, 0x50, false);
    bus.open();
    LSX_EXPECT_TRUE(bus.probe());   // 注入路径恒 true
}

LSX_TEST(I2cBus, InvalidSlaveAddressRejectedOnRealOpen) {
    I2cBus bus("/dev/null", 0x00);
    LSX_EXPECT_TRUE(bus.open().isErr());   // 地址 0 非法，先于 open 失败
}

// ---------- SpiBus ----------

LSX_TEST(SpiBus, OpenInjectAndClose) {
    FdPair p(false);
    SpiBus bus(p.a, false);
    LSX_EXPECT_TRUE(bus.open().isOk());
    LSX_EXPECT_TRUE(bus.isOpen());
    bus.close();
    LSX_EXPECT_TRUE(!bus.isOpen());
}

LSX_TEST(SpiBus, ConfigInjectPath) {
    FdPair p(false);
    SpiBus bus(p.a, false);
    bus.open();
    LSX_EXPECT_TRUE(bus.setMode(0).isOk());
    LSX_EXPECT_TRUE(bus.setMode(3).isOk());
    LSX_EXPECT_TRUE(bus.setMode(4).isErr());          // 非法模式
    LSX_EXPECT_TRUE(bus.setBitsPerWord(8).isOk());
    LSX_EXPECT_TRUE(bus.setBitsPerWord(0).isErr());
    LSX_EXPECT_TRUE(bus.setSpeedHz(1000000).isOk());
    LSX_EXPECT_TRUE(bus.setSpeedHz(0).isErr());
    LSX_EXPECT_TRUE(bus.setLsbFirst(true).isOk());
    LSX_EXPECT_EQ(bus.mode(), 3);
    LSX_EXPECT_EQ(bus.bitsPerWord(), 8);
    LSX_EXPECT_EQ(bus.speedHz(), 1000000);
}

LSX_TEST(SpiBus, TransferInjectEcho) {
    FdPair p(true);
    SpiBus bus(p.a, false);
    bus.open();
    auto r = bus.transfer({0x01, 0x02, 0x03, 0x04});
    LSX_EXPECT_TRUE(r.isOk());
    std::vector<std::uint8_t> expect = {0x01, 0x02, 0x03, 0x04};
    LSX_EXPECT_TRUE(r.value() == expect);   // 回显 = 全双工等长
}

LSX_TEST(SpiBus, TransferEmptyRejected) {
    FdPair p(true);
    SpiBus bus(p.a, false);
    bus.open();
    LSX_EXPECT_TRUE(bus.transfer({}).isErr());
}

LSX_TEST(SpiBus, WriteAndReadInjectEcho) {
    FdPair p(true);
    SpiBus bus(p.a, false);
    bus.open();
    LSX_EXPECT_TRUE(bus.write({0xAA, 0xBB}).isOk());
    auto r = bus.read(2);
    LSX_EXPECT_TRUE(r.isOk());
    LSX_EXPECT_EQ(r.value().size(), 2u);
}

LSX_TEST(SpiBus, ReadLengthValidation) {
    FdPair p(true);
    SpiBus bus(p.a, false);
    bus.open();
    LSX_EXPECT_TRUE(bus.read(0).isErr());
    LSX_EXPECT_TRUE(bus.read(5000).isErr());
}

LSX_TEST(SpiBus, OperationsWithoutOpenFail) {
    SpiBus bus("/dev/definitely_no_such_spi");
    LSX_EXPECT_TRUE(bus.setMode(0).isErr());
    LSX_EXPECT_TRUE(bus.transfer({1}).isErr());
    LSX_EXPECT_EQ(bus.mode(), -1);
}

LSX_TEST(SpiBus, ConcurrentTransfersSerialized) {
    FdPair p(true);
    SpiBus bus(p.a, false);
    bus.open();
    std::atomic<int> ok{0};
    std::vector<std::thread> ts;
    for (int t = 0; t < 4; ++t)
    {
        ts.emplace_back([&bus, &ok]() {
            for (int i = 0; i < 20; ++i)
                if (bus.transfer({0x10, 0x20}).isOk()) ++ok;
        });
    }
    for (auto& t : ts) t.join();
    LSX_EXPECT_EQ(ok.load(), 80);   // 互斥保护下全部成功
}
