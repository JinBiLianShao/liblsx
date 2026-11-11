/**
 * @file Bus.cpp
 * @brief I2C / SPI Linux 用户态实现
 */
#include "Bus/Bus.h"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <linux/spi/spidev.h>

namespace LSX_LIB {
namespace Bus {

namespace
{
std::string errnoMsg(const char* what, int e = errno)
{
    return std::string(what) + ": " + std::strerror(e) + " (errno=" + std::to_string(e) + ")";
}
} // namespace

// ==================== I2cBus ====================

OpResult I2cBus::open()
{
    if (injected_) return OpResult();
    if (fd_ >= 0) return OpResult();
    if (slave_addr_ == 0 || slave_addr_ > 0x7F)
        return Result::Err(std::string("invalid 7-bit I2C slave address"));
    int fd = ::open(device_.c_str(), O_RDWR | O_CLOEXEC);
    if (fd < 0) return Result::Err(errnoMsg("open i2c device failed"));
    fd_ = fd;
    own_fd_ = true;
    return OpResult();
}

void I2cBus::close()
{
    std::lock_guard<std::mutex> lk(mtx_);
    if (fd_ >= 0 && own_fd_) ::close(fd_);
    fd_ = -1;
}

bool I2cBus::doRdwr(std::vector<std::uint8_t>& write_buf,
                    std::vector<std::uint8_t>& read_buf, bool has_read)
{
    if (fd_ < 0) return false;
    // 真实设备上使用 I2C_RDWR ioctl；注入 fd（非字符设备）时回退到 write/read，
    // 便于 CI 用 socketpair 验证上层逻辑。
    if (injected_)
    {
        if (!write_buf.empty())
        {
            ssize_t n = ::write(fd_, write_buf.data(), write_buf.size());
            if (n < 0 || static_cast<std::size_t>(n) != write_buf.size()) return false;
        }
        if (has_read && !read_buf.empty())
        {
            std::size_t got = 0;
            while (got < read_buf.size())
            {
                ssize_t n = ::read(fd_, read_buf.data() + got, read_buf.size() - got);
                if (n <= 0) return false;
                got += static_cast<std::size_t>(n);
            }
        }
        return true;
    }

    struct i2c_msg msgs[2];
    int n = 0;
    if (!write_buf.empty())
    {
        msgs[n].addr = slave_addr_;
        msgs[n].flags = 0;
        msgs[n].len = static_cast<std::uint16_t>(write_buf.size());
        msgs[n].buf = write_buf.data();
        ++n;
    }
    if (has_read)
    {
        msgs[n].addr = slave_addr_;
        msgs[n].flags = I2C_M_RD;
        msgs[n].len = static_cast<std::uint16_t>(read_buf.size());
        msgs[n].buf = read_buf.data();
        ++n;
    }
    if (n == 0) return true;
    struct i2c_rdwr_ioctl_data data{};
    data.msgs = msgs;
    data.nmsgs = static_cast<std::uint32_t>(n);
    return ::ioctl(fd_, I2C_RDWR, &data) >= 0;
}

OpResult I2cBus::writeRegister8(std::uint8_t reg, std::uint8_t value)
{
    std::lock_guard<std::mutex> lk(mtx_);
    if (fd_ < 0) return Result::Err(std::string("I2C bus not open"));
    std::vector<std::uint8_t> w = {reg, value};
    std::vector<std::uint8_t> r;
    if (!doRdwr(w, r, false)) return Result::Err(errnoMsg("writeRegister8 failed"));
    return OpResult();
}

Result::Result<std::uint8_t> I2cBus::readRegister8(std::uint8_t reg)
{
    std::lock_guard<std::mutex> lk(mtx_);
    if (fd_ < 0) return Result::Err(std::string("I2C bus not open"));
    std::vector<std::uint8_t> w = {reg};
    std::vector<std::uint8_t> r(1);
    if (!doRdwr(w, r, true)) return Result::Err(errnoMsg("readRegister8 failed"));
    return Result::Ok(r[0]);
}

OpResult I2cBus::writeRegister16(std::uint16_t reg, std::uint8_t value)
{
    std::lock_guard<std::mutex> lk(mtx_);
    if (fd_ < 0) return Result::Err(std::string("I2C bus not open"));
    std::vector<std::uint8_t> w = {
        static_cast<std::uint8_t>(reg >> 8), static_cast<std::uint8_t>(reg & 0xFF), value};
    std::vector<std::uint8_t> r;
    if (!doRdwr(w, r, false)) return Result::Err(errnoMsg("writeRegister16 failed"));
    return OpResult();
}

Result::Result<std::uint8_t> I2cBus::readRegister16(std::uint16_t reg)
{
    std::lock_guard<std::mutex> lk(mtx_);
    if (fd_ < 0) return Result::Err(std::string("I2C bus not open"));
    std::vector<std::uint8_t> w = {
        static_cast<std::uint8_t>(reg >> 8), static_cast<std::uint8_t>(reg & 0xFF)};
    std::vector<std::uint8_t> r(1);
    if (!doRdwr(w, r, true)) return Result::Err(errnoMsg("readRegister16 failed"));
    return Result::Ok(r[0]);
}

Result::Result<std::vector<std::uint8_t>> I2cBus::readBlock(std::uint8_t reg, std::size_t len)
{
    std::lock_guard<std::mutex> lk(mtx_);
    if (fd_ < 0) return Result::Err(std::string("I2C bus not open"));
    if (len == 0 || len > 8192) return Result::Err(std::string("invalid block length"));
    std::vector<std::uint8_t> w = {reg};
    std::vector<std::uint8_t> r(len);
    if (!doRdwr(w, r, true)) return Result::Err(errnoMsg("readBlock failed"));
    return Result::Ok(std::move(r));
}

OpResult I2cBus::writeBlock(std::uint8_t reg, const std::vector<std::uint8_t>& data)
{
    std::lock_guard<std::mutex> lk(mtx_);
    if (fd_ < 0) return Result::Err(std::string("I2C bus not open"));
    if (data.empty() || data.size() > 8191) return Result::Err(std::string("invalid block length"));
    std::vector<std::uint8_t> w;
    w.reserve(1 + data.size());
    w.push_back(reg);
    w.insert(w.end(), data.begin(), data.end());
    std::vector<std::uint8_t> r;
    if (!doRdwr(w, r, false)) return Result::Err(errnoMsg("writeBlock failed"));
    return OpResult();
}

OpResult I2cBus::writeRaw(const std::vector<std::uint8_t>& data)
{
    std::lock_guard<std::mutex> lk(mtx_);
    if (fd_ < 0) return Result::Err(std::string("I2C bus not open"));
    if (data.empty()) return Result::Err(std::string("empty write"));
    std::vector<std::uint8_t> w = data;
    std::vector<std::uint8_t> r;
    if (!doRdwr(w, r, false)) return Result::Err(errnoMsg("writeRaw failed"));
    return OpResult();
}

Result::Result<std::vector<std::uint8_t>> I2cBus::readRaw(std::size_t len)
{
    std::lock_guard<std::mutex> lk(mtx_);
    if (fd_ < 0) return Result::Err(std::string("I2C bus not open"));
    if (len == 0 || len > 8192) return Result::Err(std::string("invalid read length"));
    std::vector<std::uint8_t> w;
    std::vector<std::uint8_t> r(len);
    if (!doRdwr(w, r, true)) return Result::Err(errnoMsg("readRaw failed"));
    return Result::Ok(std::move(r));
}

bool I2cBus::probe()
{
    std::lock_guard<std::mutex> lk(mtx_);
    if (fd_ < 0) return false;
    if (injected_) return true;
    return ::ioctl(fd_, I2C_SLAVE, slave_addr_) >= 0;
}

// ==================== SpiBus ====================

OpResult SpiBus::open()
{
    if (injected_) return OpResult();
    if (fd_ >= 0) return OpResult();
    int fd = ::open(device_.c_str(), O_RDWR | O_CLOEXEC);
    if (fd < 0) return Result::Err(errnoMsg("open spi device failed"));
    fd_ = fd;
    own_fd_ = true;
    return OpResult();
}

void SpiBus::close()
{
    std::lock_guard<std::mutex> lk(mtx_);
    if (fd_ >= 0 && own_fd_) ::close(fd_);
    fd_ = -1;
}

OpResult SpiBus::setMode(std::uint8_t mode)
{
    std::lock_guard<std::mutex> lk(mtx_);
    if (fd_ < 0) return Result::Err(std::string("SPI bus not open"));
    if (mode > 3) return Result::Err(std::string("SPI mode must be 0..3"));
    if (!injected_ && ::ioctl(fd_, SPI_IOC_WR_MODE, &mode) < 0)
        return Result::Err(errnoMsg("setMode failed"));
    mode_ = mode;
    return OpResult();
}

OpResult SpiBus::setBitsPerWord(std::uint8_t bits)
{
    std::lock_guard<std::mutex> lk(mtx_);
    if (fd_ < 0) return Result::Err(std::string("SPI bus not open"));
    if (bits == 0) return Result::Err(std::string("bits per word must be > 0"));
    if (!injected_ && ::ioctl(fd_, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0)
        return Result::Err(errnoMsg("setBitsPerWord failed"));
    bits_ = bits;
    return OpResult();
}

OpResult SpiBus::setSpeedHz(std::uint32_t hz)
{
    std::lock_guard<std::mutex> lk(mtx_);
    if (fd_ < 0) return Result::Err(std::string("SPI bus not open"));
    if (hz == 0) return Result::Err(std::string("speed must be > 0"));
    if (!injected_ && ::ioctl(fd_, SPI_IOC_WR_MAX_SPEED_HZ, &hz) < 0)
        return Result::Err(errnoMsg("setSpeedHz failed"));
    speed_hz_ = hz;
    return OpResult();
}

OpResult SpiBus::setLsbFirst(bool lsb)
{
    std::lock_guard<std::mutex> lk(mtx_);
    if (fd_ < 0) return Result::Err(std::string("SPI bus not open"));
    std::uint8_t v = lsb ? 1 : 0;
    if (!injected_ && ::ioctl(fd_, SPI_IOC_WR_LSB_FIRST, &v) < 0)
        return Result::Err(errnoMsg("setLsbFirst failed"));
    return OpResult();
}

int SpiBus::mode() const
{
    std::lock_guard<std::mutex> lk(mtx_);
    if (injected_) return mode_;
    if (fd_ < 0) return -1;
    std::uint8_t m = 0;
    return ::ioctl(fd_, SPI_IOC_RD_MODE, &m) >= 0 ? m : -1;
}

int SpiBus::bitsPerWord() const
{
    std::lock_guard<std::mutex> lk(mtx_);
    if (injected_) return bits_;
    if (fd_ < 0) return -1;
    std::uint8_t b = 0;
    return ::ioctl(fd_, SPI_IOC_RD_BITS_PER_WORD, &b) >= 0 ? b : -1;
}

int SpiBus::speedHz() const
{
    std::lock_guard<std::mutex> lk(mtx_);
    if (injected_) return static_cast<int>(speed_hz_);
    if (fd_ < 0) return -1;
    std::uint32_t h = 0;
    return ::ioctl(fd_, SPI_IOC_RD_MAX_SPEED_HZ, &h) >= 0 ? static_cast<int>(h) : -1;
}

Result::Result<std::vector<std::uint8_t>> SpiBus::transfer(
    const std::vector<std::uint8_t>& tx, std::uint32_t speed_hz)
{
    std::lock_guard<std::mutex> lk(mtx_);
    if (fd_ < 0) return Result::Err(std::string("SPI bus not open"));
    if (tx.empty()) return Result::Err(std::string("empty transfer"));
    std::vector<std::uint8_t> rx(tx.size());

    if (injected_)
    {
        // 注入 fd：把 tx 写出并读回等长数据（CI 用 socketpair 回显）
        ssize_t n = ::write(fd_, tx.data(), tx.size());
        if (n < 0 || static_cast<std::size_t>(n) != tx.size())
            return Result::Err(errnoMsg("injected transfer write failed"));
        std::size_t got = 0;
        while (got < rx.size())
        {
            n = ::read(fd_, rx.data() + got, rx.size() - got);
            if (n <= 0) return Result::Err(errnoMsg("injected transfer read failed"));
            got += static_cast<std::size_t>(n);
        }
        return Result::Ok(std::move(rx));
    }

    struct spi_ioc_transfer tr{};
    tr.tx_buf = reinterpret_cast<unsigned long>(tx.data());
    tr.rx_buf = reinterpret_cast<unsigned long>(rx.data());
    tr.len = static_cast<std::uint32_t>(tx.size());
    tr.speed_hz = speed_hz ? speed_hz : speed_hz_;
    tr.bits_per_word = bits_;
    tr.delay_usecs = 0;
    tr.cs_change = 0;
    if (::ioctl(fd_, SPI_IOC_MESSAGE(1), &tr) < 0)
        return Result::Err(errnoMsg("SPI transfer failed"));
    return Result::Ok(std::move(rx));
}

OpResult SpiBus::write(const std::vector<std::uint8_t>& tx)
{
    auto r = transfer(tx);
    if (r.isErr()) return Result::Err(r.error());
    return OpResult();
}

Result::Result<std::vector<std::uint8_t>> SpiBus::read(std::size_t len)
{
    if (len == 0 || len > 4096) return Result::Err(std::string("invalid read length"));
    std::vector<std::uint8_t> zeros(len, 0);
    return transfer(zeros);
}

} // namespace Bus
} // namespace LSX_LIB
