/**
 * @file Bus.h
 * @brief LIBLSX 工具库 - 嵌入式总线封装（I2C / SPI，Linux 用户态）
 * @details 提供 LSX_LIB::Bus 命名空间下基于 Linux /dev 字符设备的总线封装：
 *  - I2cBus：/dev/i2c-N，基于 I2C_RDWR ioctl 的寄存器读写（8/16 位寄存器地址）
 *  - SpiBus：/dev/spidevX.Y，基于 SPI_IOC_MESSAGE 的全双工传输与模式配置
 *
 * 面向嵌入式 Linux，特性：
 *  - RAII 管理 fd，析构自动关闭
 *  - 所有操作返回 Result（见 Result 模块），错误携带 errno 说明
 *  - **可注入 fd**：构造时可传入已打开的 fd（测试用），也可 open() 打开设备，
 *    使 CI 无需真实硬件即可用 socketpair/pipe 验证逻辑
 *  - 线程安全（每次 ioctl 加互斥锁）
 *
 * @author 连思鑫（jinbilianshao）
 * @date 2026-11-11
 * @version 1.0
 *
 * ### 使用示例
 * @code
 * #include "Bus/Bus.h"
 *
 * LSX_LIB::Bus::I2cBus i2c("/dev/i2c-1", 0x48);   // 从机地址 0x48
 * if (auto r = i2c.open()) { }
 * auto temp = i2c.readRegister8(0x00);   // Result<uint8_t>
 * if (temp.isOk()) use(temp.value());
 *
 * LSX_LIB::Bus::SpiBus spi("/dev/spidev0.0");
 * spi.open();
 * spi.setMode(0); spi.setSpeedHz(1000000);
 * auto rx = spi.transfer({0x01, 0x02});   // Result<vector<uint8_t>>
 * @endcode
 */
#pragma once

#include <utility>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include "Result/Result.h"

namespace LSX_LIB {
namespace Bus {

using OpResult = Result::Result<void, std::string>;

/**
 * @class I2cBus
 * @brief Linux I2C 从设备封装（/dev/i2c-N）
 *
 * 采用 I2C_RDWR 复合消息实现"写寄存器地址 + 读/写数据"的标准时序，
 * 支持 8 位与 16 位寄存器地址（大端序，符合绝大多数传感器约定）。
 */
class I2cBus
{
public:
    /// @param device 设备路径，如 "/dev/i2c-1"
    /// @param slave_addr 7 位从机地址（0x03~0x77）
    explicit I2cBus(std::string device, std::uint8_t slave_addr)
        : device_(std::move(device)), slave_addr_(slave_addr) {}

    /// 测试用构造：注入已打开的 fd（不拥有所有权时传 own=false）
    explicit I2cBus(int fd, std::uint8_t slave_addr, bool own = false)
        : slave_addr_(slave_addr), fd_(fd), own_fd_(own), injected_(true) {}

    ~I2cBus() { close(); }

    I2cBus(const I2cBus&) = delete;
    I2cBus& operator=(const I2cBus&) = delete;

    /// 打开设备（注入 fd 时直接返回成功）
    OpResult open();
    void close();
    bool isOpen() const { return fd_ >= 0; }

    /// 写 8 位寄存器地址 + 1 字节数据
    OpResult writeRegister8(std::uint8_t reg, std::uint8_t value);
    /// 读 8 位寄存器地址 → 1 字节
    Result::Result<std::uint8_t> readRegister8(std::uint8_t reg);

    /// 16 位寄存器地址（大端）
    OpResult writeRegister16(std::uint16_t reg, std::uint8_t value);
    Result::Result<std::uint8_t> readRegister16(std::uint16_t reg);

    /// 连续读（8 位寄存器地址起，读 len 字节）
    Result::Result<std::vector<std::uint8_t>> readBlock(std::uint8_t reg, std::size_t len);
    /// 连续写（8 位寄存器地址起）
    OpResult writeBlock(std::uint8_t reg, const std::vector<std::uint8_t>& data);

    /// 裸写（无寄存器地址，直接发数据）
    OpResult writeRaw(const std::vector<std::uint8_t>& data);
    /// 裸读
    Result::Result<std::vector<std::uint8_t>> readRaw(std::size_t len);

    /// 探测从机是否存在（发送 0 字节写）
    bool probe();

    std::uint8_t slaveAddress() const { return slave_addr_; }

private:
    /// 执行一次 I2C_RDWR；成功返回 true
    bool doRdwr(std::vector<std::uint8_t>& write_buf,
                std::vector<std::uint8_t>& read_buf, bool has_read);

    std::string device_;
    std::uint8_t slave_addr_ = 0;
    int fd_ = -1;   // 声明顺序与上面构造函数的初始化列表一致
    bool own_fd_ = true;
    bool injected_ = false;
    std::mutex mtx_;
};

/**
 * @class SpiBus
 * @brief Linux SPI 设备封装（/dev/spidevX.Y）
 */
class SpiBus
{
public:
    explicit SpiBus(std::string device)
        : device_(std::move(device)) {}

    /// 测试用构造：注入已打开的 fd
    explicit SpiBus(int fd, bool own = false)
        : fd_(fd), own_fd_(own), injected_(true) {}

    ~SpiBus() { close(); }

    SpiBus(const SpiBus&) = delete;
    SpiBus& operator=(const SpiBus&) = delete;

    OpResult open();
    void close();
    bool isOpen() const { return fd_ >= 0; }

    /// SPI 模式（0~3，对应 CPOL/CPHA 组合）
    OpResult setMode(std::uint8_t mode);
    /// 每字节位数（通常 8）
    OpResult setBitsPerWord(std::uint8_t bits);
    /// 时钟频率（Hz）
    OpResult setSpeedHz(std::uint32_t hz);
    /// 位序：0=MSB first，1=LSB first
    OpResult setLsbFirst(bool lsb);

    /// 获取以上配置的当前值（-1 表示读取失败）
    int mode() const;
    int bitsPerWord() const;
    int speedHz() const;

    /// 全双工传输：发送 tx，返回等长的接收数据（tx 为空时发 0）
    Result::Result<std::vector<std::uint8_t>> transfer(
        const std::vector<std::uint8_t>& tx, std::uint32_t speed_hz = 0);

    /// 半双工写（丢弃接收数据）
    OpResult write(const std::vector<std::uint8_t>& tx);
    /// 半双工读（发送 0，返回 len 字节）
    Result::Result<std::vector<std::uint8_t>> read(std::size_t len);

private:
    std::string device_;
    int fd_ = -1;
    bool own_fd_ = true;
    bool injected_ = false;
    std::uint32_t speed_hz_ = 1000000;
    std::uint8_t bits_ = 8;
    std::uint8_t mode_ = 0;
    mutable std::mutex mtx_;   // const getter 需要加锁
};

} // namespace Bus
} // namespace LSX_LIB
