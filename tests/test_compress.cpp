/**
 * @file test_compress.cpp
 * @brief Compress 模块深度测试：DEFLATE/zlib/gzip 回环、RLE、校验和
 */
#include "test_framework.h"

#include <string>
#include <vector>

#include "Compress/Compress.h"
#include "Checksum/Checksum.h"

using namespace LSX_LIB::Compress;

static std::vector<std::uint8_t> textData(std::size_t n)
{
    const std::string words = "the quick brown fox jumps over the lazy dog ";
    std::vector<std::uint8_t> d;
    d.reserve(n);
    while (d.size() < n)
        for (char c : words) { d.push_back(static_cast<std::uint8_t>(c)); if (d.size() >= n) break; }
    d.resize(n);
    return d;
}

LSX_TEST(Compress, DeflateInflateRoundtrip) {
    auto data = textData(4096);
    auto c = deflate(data);
    LSX_EXPECT_TRUE(!c.empty());
    LSX_EXPECT_TRUE(c.size() < data.size());   // 高重复文本应明显压缩
    auto back = inflate(c, data.size());
    LSX_EXPECT_TRUE(back == data);
}

LSX_TEST(Compress, DeflateEmptyReturnsEmpty) {
    LSX_EXPECT_TRUE(deflate(std::vector<std::uint8_t>{}).empty());
    LSX_EXPECT_TRUE(inflate(std::vector<std::uint8_t>{}).empty());
}

LSX_TEST(Compress, InflateAutoGrowWithoutExpectedSize) {
    auto data = textData(100000);
    auto c = deflate(data);
    auto back = inflate(c);   // expected_size = 0，验证自动扩容
    LSX_EXPECT_TRUE(back == data);
}

LSX_TEST(Compress, LevelAffectsSize) {
    auto data = textData(20000);
    auto c0 = deflate(data, Level::NoCompression);
    auto c9 = deflate(data, Level::Best);
    LSX_EXPECT_TRUE(c0.size() > c9.size());
    LSX_EXPECT_TRUE(inflate(c0, data.size()) == data);
    LSX_EXPECT_TRUE(inflate(c9, data.size()) == data);
}

LSX_TEST(Compress, ZlibRoundtrip) {
    auto data = textData(8192);
    auto c = zlibCompress(data);
    LSX_EXPECT_TRUE(c.size() >= 6u);              // 2 头 + 4 Adler32 尾
    LSX_EXPECT_EQ(c[0], std::uint8_t(0x78));      // CMF：deflate/32K window
    auto back = zlibDecompress(c);
    LSX_EXPECT_TRUE(back == data);
}

LSX_TEST(Compress, ZlibRejectsCorrupted) {
    auto c = zlibCompress(textData(100));
    LSX_EXPECT_TRUE(!c.empty());
    c[c.size() / 2] ^= 0xFF;
    LSX_EXPECT_TRUE(zlibDecompress(c).empty());
}

LSX_TEST(Compress, GzipRoundtripAndHeader) {
    auto data = textData(4096);
    auto g = gzipCompress(data);
    LSX_EXPECT_TRUE(g.size() > 18u);
    LSX_EXPECT_EQ(g[0], std::uint8_t(0x1F));
    LSX_EXPECT_EQ(g[1], std::uint8_t(0x8B));
    LSX_EXPECT_EQ(g[2], std::uint8_t(8));
    auto back = gzipDecompress(g);
    LSX_EXPECT_TRUE(back == data);
}

LSX_TEST(Compress, GzipRejectsCorruptedCrc) {
    auto g = gzipCompress(textData(100));
    g[g.size() - 5] ^= 0xFF;    // 篡改 CRC32 尾
    LSX_EXPECT_TRUE(gzipDecompress(g).empty());
}

LSX_TEST(Compress, RleRoundtrip) {
    std::vector<std::uint8_t> data = {1,1,1,1,2,2,3,3,3,3,3,0,0,0,0,0,0,0,0};
    auto c = rleEncode(data);
    LSX_EXPECT_TRUE(c.size() < data.size());   // 重复度高 → 压缩
    auto back = rleDecode(c);
    LSX_EXPECT_TRUE(back == data);
}

LSX_TEST(Compress, RleSingleRunAndEmpty) {
    LSX_EXPECT_TRUE(rleEncode(std::vector<std::uint8_t>{}).empty());
    std::vector<std::uint8_t> single(10, 0x77);
    auto c = rleEncode(single);
    LSX_EXPECT_EQ(c.size(), 2u);
    LSX_EXPECT_TRUE(rleDecode(c) == single);
}

LSX_TEST(Compress, RleLongRunSplitsAt255) {
    std::vector<std::uint8_t> data(600, 0xAB);
    auto c = rleEncode(data);
    LSX_EXPECT_TRUE(c.size() >= 6u);   // 至少 3 个 [value,count] 对
    LSX_EXPECT_TRUE(rleDecode(c) == data);
}

LSX_TEST(Compress, RleRejectsOddAndZeroCount) {
    std::vector<std::uint8_t> odd = {1, 2, 3};
    LSX_EXPECT_TRUE(rleDecode(odd).empty());
    std::vector<std::uint8_t> zero = {1, 0};
    LSX_EXPECT_TRUE(rleDecode(zero).empty());
}

LSX_TEST(Compress, Crc32MatchesChecksumModule) {
    auto d = textData(1000);
    LSX_EXPECT_EQ(crc32(d.data(), d.size()),
                  LSX_LIB::Checksum::Crc32::calc(d.data(), d.size()));
}

LSX_TEST(Compress, Adler32StandardVector) {
    // "Wikipedia" → 0x11E60398（标准示例）
    std::string s = "Wikipedia";
    LSX_EXPECT_EQ(adler32(reinterpret_cast<const std::uint8_t*>(s.data()), s.size()),
                  std::uint32_t(0x11E60398u));
}
