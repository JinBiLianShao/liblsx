/**
 * @file test_checksum.cpp
 * @brief Checksum 模块深度测试：标准向量 / 增量一致性 / 边界
 */
#include "test_framework.h"

#include <string>
#include <vector>

#include "Checksum/Checksum.h"

using namespace LSX_LIB::Checksum;

static std::vector<std::uint8_t> bytes(const std::string& s)
{
    return std::vector<std::uint8_t>(s.begin(), s.end());
}

LSX_TEST(Checksum, Crc32StandardVector) {
    // "123456789" → 0xCBF43926（CRC32/ISO-HDLC 标准向量，与 zlib 一致）
    auto d = bytes("123456789");
    LSX_EXPECT_EQ(Crc32::calc(d.data(), d.size()), std::uint32_t(0xCBF43926u));
}

LSX_TEST(Checksum, Crc32Empty) {
    LSX_EXPECT_EQ(Crc32::calc(nullptr, 0), std::uint32_t(0u));
}

LSX_TEST(Checksum, Crc16CcittFalseStandardVector) {
    // "123456789" → 0x29B1
    auto d = bytes("123456789");
    LSX_EXPECT_EQ(Crc16CcittFalse::calc(d.data(), d.size()), std::uint16_t(0x29B1u));
}

LSX_TEST(Checksum, Crc16ModbusStandardVector) {
    // Modbus 常见测试帧 01 03 00 00 00 01 → CRC 计算值 0x0A84（帧上低字节在前为 0x840A）
    std::uint8_t frame[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x01};
    Crc16Modbus c; c.update(frame, sizeof(frame));
    LSX_EXPECT_EQ(c.value(), std::uint16_t(0x0A84u));
    LSX_EXPECT_EQ(c.littleEndian(), std::uint16_t(0x840Au));
}

LSX_TEST(Checksum, Crc8StandardVector) {
    // "123456789" → 0xF4（CRC-8/SMBUS, poly 0x07, init 0）
    auto d = bytes("123456789");
    LSX_EXPECT_EQ(Crc8::calc(d.data(), d.size()), std::uint8_t(0xF4u));
}

LSX_TEST(Checksum, IncrementalEqualsOneShot) {
    auto d = bytes("The quick brown fox jumps over the lazy dog");
    Crc32 one; one.update(d.data(), d.size());
    Crc32 inc;
    for (std::size_t i = 0; i < d.size(); i += 5)
        inc.update(d.data() + i, std::min<std::size_t>(5, d.size() - i));
    LSX_EXPECT_EQ(one.value(), inc.value());
}

LSX_TEST(Checksum, ResetRestartsCalculation) {
    auto d = bytes("123456789");
    Crc32 c;
    c.update(d.data(), d.size());
    std::uint32_t first = c.value();
    c.reset();
    c.update(d.data(), d.size());
    LSX_EXPECT_EQ(first, c.value());
    c.reset();
    LSX_EXPECT_EQ(c.value(), std::uint32_t(0u));
}

LSX_TEST(Checksum, SimpleChecksums) {
    std::uint8_t d[] = {1, 2, 3, 4, 5};
    LSX_EXPECT_EQ(sum8(d, 5), std::uint8_t(15));
    LSX_EXPECT_EQ(sum8Complement(d, 5), std::uint8_t(241));
    LSX_EXPECT_EQ(xor8(d, 5), std::uint8_t(1));
    // 补码校验：整帧和为 0
    std::uint8_t full[] = {1, 2, 3, 4, 5, sum8Complement(d, 5)};
    LSX_EXPECT_EQ(sum8(full, 6), std::uint8_t(0));
}

LSX_TEST(Checksum, Fletcher16StandardVector) {
    auto d = bytes("abcde");
    LSX_EXPECT_EQ(fletcher16(d.data(), d.size()), std::uint16_t(0xC8F0u));
}

LSX_TEST(Checksum, LargeBufferDeterministic) {
    std::vector<std::uint8_t> big(64 * 1024);
    for (std::size_t i = 0; i < big.size(); ++i) big[i] = static_cast<std::uint8_t>(i * 7 + 3);
    LSX_EXPECT_EQ(Crc32::calc(big.data(), big.size()), Crc32::calc(big.data(), big.size()));
}
