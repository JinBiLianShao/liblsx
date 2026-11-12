/**
 * @file example_bus.cpp
 * @brief LIBLSX Bus 模块使用示例（I2C / SPI，Linux 用户态）
 *
 * 演示内容：
 *   1. I2cBus —— 注入 socketpair fd 做读写回显，免真实硬件演示寄存器读写逻辑
 *   2. SpiBus —— 注入 fd 演示全双工 transfer 的上层逻辑
 *
 * 关键点：
 *   - 真实硬件：I2cBus(设备路径, 从机地址).open()，SpiBus(设备路径).open()。
 *   - 注入 fd（构造传已打开的 fd）让 CI / 无硬件环境也能跑通上层逻辑。
 *   - 注入模式下 I2C 回退到 write/read、SPI 回退到 write 后读回等长数据，
 *     因此需要另一端有一个回显线程，把收到的字节原样写回（模拟硬件响应）。
 *   - 所有操作返回 Result，错误携带 errno 说明；isOk()/isErr() 判断。
 */

#include "Bus/Bus.h"

#include <atomic>
#include <iostream>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

namespace bus = LSX_LIB::Bus;

int main()
{
    std::cout << "===== Bus（I2C/SPI）示例 =====\n";

    // ---- 1) 造一对 socketpair，另一端起回显线程模拟硬件 ----
    int sv[2];
    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) {
        std::cerr << "  socketpair 失败\n";
        return 1;
    }

    // 回显线程：从 sv[1] 读多少字节，就原样写回多少字节。
    // 这样 sv[0] 端（注入给总线的 fd）的 read 才能拿到数据，模拟硬件响应。
    std::atomic<bool> echo_run{true};
    std::thread echo([&]() {
        std::uint8_t buf[256];
        while (echo_run.load()) {
            ssize_t n = ::read(sv[1], buf, sizeof(buf));
            if (n <= 0) break;
            ::write(sv[1], buf, static_cast<std::size_t>(n));  // 原样回显
        }
    });

    // ---- 2) I2cBus：注入 sv[0]（own=false，不接管 fd 所有权）----
    bus::I2cBus i2c(sv[0], 0x48, false);
    if (auto r = i2c.open(); r.isOk()) {
        // 写寄存器 0x00 = 0xAB：注入模式把 [reg, value] 写到 socketpair
        if (auto w = i2c.writeRegister8(0x00, 0xAB); w.isOk()) {
            std::cout << "  [i2c] 写寄存器成功\n";
        }
        // 读寄存器 0x00：注入模式写 [reg] 后 read 回显字节。
        // 注意：注入模式退化为「write 后 read 流」，回读值是回显流的第一个字节；
        // 真实硬件走 I2C_RDWR ioctl，会原子地返回目标寄存器内容。
        if (auto v = i2c.readRegister8(0x00); v.isOk()) {
            std::cout << "  [i2c] 读寄存器 = 0x" << std::hex
                      << static_cast<int>(v.value()) << std::dec << "\n";
        }
        i2c.close();
    } else {
        std::cout << "  [i2c] open 失败: " << r.error() << "\n";
    }

    // ---- 3) SpiBus：同样注入 sv[0]（回显线程在另一端）----
    bus::SpiBus spi(sv[0], false);
    if (auto r = spi.open(); r.isOk()) {
        auto rx = spi.transfer({0x01, 0x02, 0x03});
        if (rx.isOk()) {
            std::cout << "  [spi] transfer 回读 " << rx.value().size() << " 字节\n";
        }
        spi.close();
    }

    // ---- 4) 收尾 ----
    echo_run.store(false);
    ::shutdown(sv[1], SHUT_RDWR);   // 唤醒回显线程的 read 使其退出
    echo.join();
    ::close(sv[0]);
    ::close(sv[1]);

    std::cout << "  提示：真实硬件请把构造参数换成 /dev/i2c-1 或 /dev/spidev0.0 路径\n";
    return 0;
}
