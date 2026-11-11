/**
 * @file Checksum.h
 * @brief LIBLSX 工具库 - 校验算法模块（嵌入式通信常用）
 * @details 提供 LSX_LIB::Checksum 命名空间下的纯头文件校验算法：
 *  - CRC8（多项式 0x07，init 0x00）
 *  - CRC16/Modbus（多项式 0xA001，init 0xFFFF，地位先传）
 *  - CRC16/CCITT-FALSE（多项式 0x1021，init 0xFFFF）
 *  - CRC32/ISO-HDLC（多项式 0xEDB88320，与 zlib 一致）
 *  - 累加和、异或校验、补码校验、Fletcher16
 *
 * 全部为增量式接口（update 可分段喂数据），适合流式帧校验。
 * 表在首次使用时由 constexpr 生成，无全局初始化、无动态分配。
 *
 * @author 连思鑫（jinbilianshao）
 * @date 2026-11-11
 * @version 1.0
 */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace LSX_LIB {
namespace Checksum {

namespace detail {

template <std::size_t N, typename Generator>
constexpr auto makeTable(Generator gen)
{
    std::array<std::uint32_t, N> t{};
    for (std::uint32_t i = 0; i < N; ++i) t[i] = gen(i);
    return t;
}

// CRC8: poly 0x07 (x^8+x^2+x+1), MSB-first
inline constexpr auto crc8_table = makeTable<256>([](std::uint32_t i) {
    std::uint32_t c = i;
    for (int k = 0; k < 8; ++k) c = (c & 0x80) ? ((c << 1) ^ 0x07) : (c << 1);
    return c & 0xFF;
});

// CRC32/ISO-HDLC reflected poly 0xEDB88320
inline constexpr auto crc32_table = makeTable<256>([](std::uint32_t i) {
    std::uint32_t c = i;
    for (int k = 0; k < 8; ++k) c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
    return c;
});

// CRC16/Modbus reflected poly 0xA001
inline constexpr auto crc16_modbus_table = makeTable<256>([](std::uint32_t i) {
    std::uint32_t c = i;
    for (int k = 0; k < 8; ++k) c = (c & 1) ? (0xA001u ^ (c >> 1)) : (c >> 1);
    return c & 0xFFFF;
});

// CRC16/CCITT-FALSE poly 0x1021, MSB-first
inline constexpr auto crc16_ccitt_table = makeTable<256>([](std::uint32_t i) {
    std::uint32_t c = i << 8;
    for (int k = 0; k < 8; ++k) c = (c & 0x8000) ? ((c << 1) ^ 0x1021u) : (c << 1);
    return c & 0xFFFF;
});

} // namespace detail

// ---------- CRC8 ----------

class Crc8
{
public:
    void reset() { crc_ = 0; }
    void update(const std::uint8_t* data, std::size_t len)
    {
        for (std::size_t i = 0; i < len; ++i)
            crc_ = static_cast<std::uint8_t>(detail::crc8_table[(crc_ ^ data[i]) & 0xFF]);
    }
    std::uint8_t value() const { return crc_; }
    static std::uint8_t calc(const std::uint8_t* d, std::size_t n)
    {
        Crc8 c; c.update(d, n); return c.value();
    }
private:
    std::uint8_t crc_ = 0;
};

// ---------- CRC16 Modbus ----------

class Crc16Modbus
{
public:
    void reset() { crc_ = 0xFFFF; }
    void update(const std::uint8_t* data, std::size_t len)
    {
        for (std::size_t i = 0; i < len; ++i)
            crc_ = static_cast<std::uint16_t>((crc_ >> 8) ^
                detail::crc16_modbus_table[(crc_ ^ data[i]) & 0xFF]);
    }
    std::uint16_t value() const { return crc_; }
    /// Modbus 帧中的字节序：低字节在前
    std::uint16_t littleEndian() const
    {
        return static_cast<std::uint16_t>((crc_ << 8) | (crc_ >> 8));
    }
    static std::uint16_t calc(const std::uint8_t* d, std::size_t n)
    {
        Crc16Modbus c; c.update(d, n); return c.value();
    }
private:
    std::uint16_t crc_ = 0xFFFF;
};

// ---------- CRC16 CCITT-FALSE ----------

class Crc16CcittFalse
{
public:
    void reset() { crc_ = 0xFFFF; }
    void update(const std::uint8_t* data, std::size_t len)
    {
        for (std::size_t i = 0; i < len; ++i)
            crc_ = static_cast<std::uint16_t>((crc_ << 8) ^
                detail::crc16_ccitt_table[((crc_ >> 8) ^ data[i]) & 0xFF]);
    }
    std::uint16_t value() const { return crc_; }
    static std::uint16_t calc(const std::uint8_t* d, std::size_t n)
    {
        Crc16CcittFalse c; c.update(d, n); return c.value();
    }
private:
    std::uint16_t crc_ = 0xFFFF;
};

// ---------- CRC32 / ISO-HDLC ----------

class Crc32
{
public:
    void reset() { crc_ = 0xFFFFFFFFu; }
    void update(const std::uint8_t* data, std::size_t len)
    {
        for (std::size_t i = 0; i < len; ++i)
            crc_ = detail::crc32_table[(crc_ ^ data[i]) & 0xFF] ^ (crc_ >> 8);
    }
    std::uint32_t value() const { return crc_ ^ 0xFFFFFFFFu; }
    static std::uint32_t calc(const std::uint8_t* d, std::size_t n)
    {
        Crc32 c; c.update(d, n); return c.value();
    }
private:
    std::uint32_t crc_ = 0xFFFFFFFFu;
};

// ---------- 简单校验 ----------

/// 累加和（8 位回卷）
inline std::uint8_t sum8(const std::uint8_t* d, std::size_t n)
{
    std::uint8_t s = 0;
    for (std::size_t i = 0; i < n; ++i) s = static_cast<std::uint8_t>(s + d[i]);
    return s;
}

/// 累加和取补（使整帧含校验字节之和为 0）
inline std::uint8_t sum8Complement(const std::uint8_t* d, std::size_t n)
{
    return static_cast<std::uint8_t>(0 - sum8(d, n));
}

/// 异或校验
inline std::uint8_t xor8(const std::uint8_t* d, std::size_t n)
{
    std::uint8_t x = 0;
    for (std::size_t i = 0; i < n; ++i) x ^= d[i];
    return x;
}

/// Fletcher-16（两个 8 位累加和模 255）
inline std::uint16_t fletcher16(const std::uint8_t* d, std::size_t n)
{
    std::uint16_t s1 = 0, s2 = 0;
    for (std::size_t i = 0; i < n; ++i)
    {
        s1 = (s1 + d[i]) % 255;
        s2 = (s2 + s1) % 255;
    }
    return static_cast<std::uint16_t>((s2 << 8) | s1);
}

} // namespace Checksum
} // namespace LSX_LIB
