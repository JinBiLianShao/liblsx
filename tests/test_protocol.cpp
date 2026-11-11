/**
 * @file test_protocol.cpp
 * @brief Protocol 模块深度测试：帧头帧尾/粘包半包/转义/校验/长度字段
 */
#include "test_framework.h"

#include <string>
#include <vector>

#include "Protocol/FrameParser.h"

using namespace LSX_LIB;
using namespace LSX_LIB::Protocol;

LSX_TEST(FrameParser, BasicHeaderEnderExtraction) {
    FrameParser p;
    p.setHeader({0xAA, 0x55});
    p.setEnder({0x0D, 0x0A});
    std::vector<std::vector<std::uint8_t>> frames;
    p.onFrame([&](const std::uint8_t* f, std::size_t n) {
        frames.emplace_back(f, f + n);
    });
    std::vector<std::uint8_t> stream = {0xAA, 0x55, 0x01, 0x02, 0x0D, 0x0A};
    p.feed(stream.data(), stream.size());

    LSX_EXPECT_EQ(frames.size(), 1u);
    LSX_EXPECT_EQ(frames[0].size(), 6u);  // 含帧头帧尾
    LSX_EXPECT_EQ(frames[0][2], std::uint8_t(1));
    LSX_EXPECT_EQ(p.framesAccepted(), 1u);
}

LSX_TEST(FrameParser, NoiseBeforeHeaderIgnored) {
    FrameParser p;
    p.setHeader({0xAA, 0x55});
    p.setEnder({0x0D, 0x0A});
    std::vector<std::vector<std::uint8_t>> frames;
    p.onFrame([&](const std::uint8_t* f, std::size_t n) { frames.emplace_back(f, f + n); });

    std::vector<std::uint8_t> stream = {0x00, 0xFF, 0x12, 0xAA, 0x55, 0x42, 0x0D, 0x0A};
    p.feed(stream.data(), stream.size());
    LSX_EXPECT_EQ(frames.size(), 1u);
    LSX_EXPECT_EQ(frames[0][0], std::uint8_t(0xAA));
    LSX_EXPECT_EQ(frames[0][2], std::uint8_t(0x42));   // 载荷在帧头之后
}

LSX_TEST(FrameParser, OverlappingPartialHeader) {
    // 帧头 AA 55，流里出现 AA AA 55 —— 部分匹配回退必须能命中
    FrameParser p;
    p.setHeader({0xAA, 0x55});
    p.setEnder({0x0D, 0x0A});
    std::vector<std::vector<std::uint8_t>> frames;
    p.onFrame([&](const std::uint8_t* f, std::size_t n) { frames.emplace_back(f, f + n); });
    std::vector<std::uint8_t> s = {0xAA, 0xAA, 0x55, 0x01, 0x0D, 0x0A};
    p.feed(s.data(), s.size());
    LSX_EXPECT_EQ(frames.size(), 1u);
    LSX_EXPECT_EQ(frames[0][2], std::uint8_t(0x01));   // 载荷 0x01
}

LSX_TEST(FrameParser, StickyPacketsMultipleFrames) {
    FrameParser p;
    p.setHeader({0xAA, 0x55});
    p.setEnder({0x0D, 0x0A});
    std::vector<std::vector<std::uint8_t>> frames;
    p.onFrame([&](const std::uint8_t* f, std::size_t n) { frames.emplace_back(f, f + n); });
    std::vector<std::uint8_t> s = {
        0xAA, 0x55, 0x01, 0x0D, 0x0A,   // 帧1
        0xAA, 0x55, 0x02, 0x03, 0x0D, 0x0A  // 帧2
    };
    p.feed(s.data(), s.size());
    LSX_EXPECT_EQ(frames.size(), 2u);
    LSX_EXPECT_EQ(frames[0][2], std::uint8_t(0x01));
    LSX_EXPECT_EQ(frames[1][2], std::uint8_t(0x02));
    LSX_EXPECT_EQ(frames[1][3], std::uint8_t(0x03));
}

LSX_TEST(FrameParser, HalfPacketAcrossFeeds) {
    FrameParser p;
    p.setHeader({0xAA, 0x55});
    p.setEnder({0x0D, 0x0A});
    std::vector<std::vector<std::uint8_t>> frames;
    p.onFrame([&](const std::uint8_t* f, std::size_t n) { frames.emplace_back(f, f + n); });
    std::vector<std::uint8_t> part1 = {0xAA, 0x55, 0x10};
    std::vector<std::uint8_t> part2 = {0x20, 0x0D, 0x0A};
    p.feed(part1.data(), part1.size());
    LSX_EXPECT_EQ(frames.size(), 0u);   // 半包不成帧
    p.feed(part2.data(), part2.size());
    LSX_EXPECT_EQ(frames.size(), 1u);   // 补齐后成帧
    LSX_EXPECT_EQ(frames[0].size(), 6u);
}

LSX_TEST(FrameParser, CheckCallbackRejectsBadFrame) {
    FrameParser p;
    p.setHeader({0xAA, 0x55});
    p.setEnder({0x0D, 0x0A});
    // 校验：内容区（帧头后、帧尾前）最后一字节必须等于前面字节的异或
    p.setCheckCallback([](const std::uint8_t* d, std::size_t n) {
        if (n == 0) return false;
        std::uint8_t x = 0;
        for (std::size_t i = 0; i + 1 < n; ++i) x ^= d[i];
        return x == d[n - 1];
    });
    std::vector<std::vector<std::uint8_t>> frames;
    p.onFrame([&](const std::uint8_t* f, std::size_t n) { frames.emplace_back(f, f + n); });

    std::vector<std::uint8_t> good = {0xAA, 0x55, 0x01, 0x02, 0x03, 0x0D, 0x0A};
    p.feed(good.data(), good.size());
    LSX_EXPECT_EQ(frames.size(), 1u);   // 1^2=3 校验通过

    std::vector<std::uint8_t> bad = {0xAA, 0x55, 0x01, 0x02, 0x09, 0x0D, 0x0A};
    p.feed(bad.data(), bad.size());
    LSX_EXPECT_EQ(frames.size(), 1u);   // 校验失败不出帧
    LSX_EXPECT_EQ(p.framesRejected(), 1u);
}

LSX_TEST(FrameParser, LengthFieldMode) {
    // 帧头 AA 55 + 1 字节长度（表示长度字段之后的字节数）+ 载荷（无帧尾）
    FrameParser p;
    p.setHeader({0xAA, 0x55});
    p.setLengthField(0, 1);
    std::vector<std::vector<std::uint8_t>> frames;
    p.onFrame([&](const std::uint8_t* f, std::size_t n) { frames.emplace_back(f, f + n); });
    std::vector<std::uint8_t> s = {0xAA, 0x55, 0x03, 0x10, 0x20, 0x30};
    p.feed(s.data(), s.size());
    LSX_EXPECT_EQ(frames.size(), 1u);
    LSX_EXPECT_EQ(frames[0].size(), 6u);
    LSX_EXPECT_EQ(frames[0][5], std::uint8_t(0x30));
}

LSX_TEST(FrameParser, LengthFieldTwoByteLittleEndian) {
    FrameParser p;
    p.setHeader({0xAA, 0x55});
    p.setLengthField(0, 2);
    std::vector<std::vector<std::uint8_t>> frames;
    p.onFrame([&](const std::uint8_t* f, std::size_t n) { frames.emplace_back(f, f + n); });
    // 长度 = 0x0100 = 256 字节载荷
    std::vector<std::uint8_t> s = {0xAA, 0x55, 0x00, 0x01};
    s.insert(s.end(), 256, 0x7E);
    p.feed(s.data(), s.size());
    LSX_EXPECT_EQ(frames.size(), 1u);
    LSX_EXPECT_EQ(frames[0].size(), 260u);
}

LSX_TEST(FrameParser, EscapeByteHandling) {
    FrameParser p;
    p.setHeader({0xAA, 0x55});
    p.setEnder({0x0D, 0x0A});
    p.setEscapeByte(0x1B);
    std::vector<std::vector<std::uint8_t>> frames;
    p.onFrame([&](const std::uint8_t* f, std::size_t n) { frames.emplace_back(f, f + n); });
    // 载荷中出现帧尾字节 0x0D，经转义后不应提前结束帧
    std::vector<std::uint8_t> s = {0xAA, 0x55, 0x01, 0x1B, 0x0D, 0x02, 0x0D, 0x0A};
    p.feed(s.data(), s.size());
    LSX_EXPECT_EQ(frames.size(), 1u);
    // 转义字节本身被剔除，0x0D 保留
    bool found = false;
    for (auto b : frames[0]) if (b == 0x0D) found = true;
    LSX_EXPECT_TRUE(found);
}

LSX_TEST(FrameParser, OversizedFrameRejected) {
    FrameParser p(16);   // 最大帧长 16
    p.setHeader({0xAA, 0x55});
    p.setEnder({0x0D, 0x0A});
    std::vector<std::uint8_t> payload(40, 0x01);
    p.feed(0xAA); p.feed(0x55);
    for (auto b : payload) p.feed(b);
    LSX_EXPECT_EQ(p.framesRejected(), 1u);
    LSX_EXPECT_EQ(p.pending(), 0u);   // 超长后重置
}

LSX_TEST(FrameParser, ResetClearsState) {
    FrameParser p;
    p.setHeader({0xAA, 0x55});
    p.setEnder({0x0D, 0x0A});
    p.feed(0xAA);
    LSX_EXPECT_TRUE(p.pending() > 0u);
    p.reset();
    LSX_EXPECT_EQ(p.pending(), 0u);
}

LSX_TEST(FrameParser, ExtractFramesBatch) {
    std::vector<std::vector<std::uint8_t>> frames;
    std::vector<std::uint8_t> s = {
        0xAA, 0x55, 0x01, 0x0D, 0x0A,
        0xAA, 0x55, 0x02, 0x0D, 0x0A,
        0xAA, 0x55, 0x03   // 尾部半包
    };
    std::size_t pending = extractFrames(s.data(), s.size(), {0xAA, 0x55}, {0x0D, 0x0A}, frames);
    LSX_EXPECT_EQ(frames.size(), 2u);
    LSX_EXPECT_EQ(pending, 3u);   // AA 55 03 留待下一批
}

LSX_TEST(FrameParser, ContentOfStripsHeaderEnder) {
    std::uint8_t frame[] = {0xAA, 0x55, 0x01, 0x02, 0x0D, 0x0A};
    const std::uint8_t* content = nullptr;
    std::size_t n = 0;
    LSX_EXPECT_TRUE(FrameParser::contentOf(frame, 6, 2, 2, &content, &n));
    LSX_EXPECT_EQ(n, 2u);
    LSX_EXPECT_EQ(content[0], std::uint8_t(0x01));
}
