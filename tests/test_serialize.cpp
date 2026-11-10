/**
 * @file test_serialize.cpp
 * @brief Serialize 模块深度测试：回环/边界/损坏输入/TLV/协变
 */
#include "test_framework.h"

#include <cmath>
#include <limits>
#include <string>
#include <vector>

#include "Serialize/Serialize.h"

using namespace LSX_LIB::Serialize;

// ---------- 固定宽度回环 ----------

LSX_TEST(Serialize, FixedWidthRoundtrip) {
    BinaryWriter w;
    w.writeU8(0xAB);
    w.writeU16(0x1234);
    w.writeU32(0xDEADBEEF);
    w.writeU64(0x0123456789ABCDEFull);
    w.writeI8(-1);
    w.writeI16(-1234);
    w.writeI32(-123456789);
    w.writeI64(-1234567890123456789LL);
    w.writeBool(true);
    w.writeBool(false);
    w.writeF32(3.14f);
    w.writeF64(2.718281828);

    BinaryReader r(w.data().data(), w.size());
    LSX_EXPECT_EQ(r.readU8(), std::uint8_t(0xAB));
    LSX_EXPECT_EQ(r.readU16(), std::uint16_t(0x1234));
    LSX_EXPECT_EQ(r.readU32(), std::uint32_t(0xDEADBEEF));
    LSX_EXPECT_EQ(r.readU64(), std::uint64_t(0x0123456789ABCDEFull));
    LSX_EXPECT_EQ(r.readI8(), std::int8_t(-1));
    LSX_EXPECT_EQ(r.readI16(), std::int16_t(-1234));
    LSX_EXPECT_EQ(r.readI32(), -123456789);
    LSX_EXPECT_EQ(r.readI64(), std::int64_t(-1234567890123456789LL));
    LSX_EXPECT_EQ(r.readBool(), true);
    LSX_EXPECT_EQ(r.readBool(), false);
    LSX_EXPECT_TRUE(std::fabs(r.readF32() - 3.14f) < 1e-5f);
    LSX_EXPECT_TRUE(std::fabs(r.readF64() - 2.718281828) < 1e-9);
    LSX_EXPECT_TRUE(!r.fail());
    LSX_EXPECT_EQ(r.remaining(), 0u);
}

LSX_TEST(Serialize, LittleEndianLayout) {
    BinaryWriter w;
    w.writeU32(0x11223344);
    // 小端：低字节在前
    const auto& d = w.data();
    LSX_EXPECT_EQ(d[0], std::uint8_t(0x44));
    LSX_EXPECT_EQ(d[1], std::uint8_t(0x33));
    LSX_EXPECT_EQ(d[2], std::uint8_t(0x22));
    LSX_EXPECT_EQ(d[3], std::uint8_t(0x11));
}

// ---------- varint 边界 ----------

LSX_TEST(Serialize, VarintBoundaries) {
    BinaryWriter w;
    std::uint64_t values[] = {
        0, 1, 127, 128, 129, 16383, 16384, 16385,
        std::numeric_limits<std::uint32_t>::max(),
        std::numeric_limits<std::uint64_t>::max()
    };
    for (std::uint64_t v : values) w.writeVarint(v);

    BinaryReader r(w.data().data(), w.size());
    for (std::uint64_t v : values)
    {
        LSX_EXPECT_EQ(r.readVarint(), v);
    }
    LSX_EXPECT_TRUE(!r.fail());
}

LSX_TEST(Serialize, VarintCompactForSmallValues) {
    BinaryWriter w;
    w.writeVarint(0);
    w.writeVarint(127);
    LSX_EXPECT_EQ(w.size(), 2u);       // 单字节编码
    w.clear();
    w.writeVarint(128);
    LSX_EXPECT_EQ(w.size(), 2u);       // 两字节
    w.clear();
    w.writeVarint(std::numeric_limits<std::uint64_t>::max());
    LSX_EXPECT_EQ(w.size(), 10u);      // 最大 10 字节
}

LSX_TEST(Serialize, VarintZZSignedRoundtrip) {
    BinaryWriter w;
    std::int64_t values[] = {0, -1, 1, -2, 2, -63, 63, -64, 64,
                             std::numeric_limits<std::int64_t>::min(),
                             std::numeric_limits<std::int64_t>::max()};
    for (std::int64_t v : values) w.writeVarintZZ(v);

    BinaryReader r(w.data().data(), w.size());
    for (std::int64_t v : values)
    {
        LSX_EXPECT_EQ(r.readVarintZZ(), v);
    }
    LSX_EXPECT_TRUE(!r.fail());
}

LSX_TEST(Serialize, VarintZZSmallNegativeCompact) {
    BinaryWriter w;
    w.writeVarintZZ(-1);               // zigzag(-1)=1 → 单字节
    LSX_EXPECT_EQ(w.size(), 1u);       // 对比直接 writeI64 要 8 字节
}

// ---------- 字符串与字节数组 ----------

LSX_TEST(Serialize, StringRoundtrip) {
    BinaryWriter w;
    std::string cases[] = {"", "hello", "中文测试", std::string(1000, 'x')};
    for (const auto& s : cases) w.writeString(s);

    BinaryReader r(w.data().data(), w.size());
    for (const auto& s : cases)
    {
        LSX_EXPECT_EQ(r.readString(), s);
    }
    LSX_EXPECT_TRUE(!r.fail());
}

LSX_TEST(Serialize, BinaryBytesWithZeros) {
    BinaryWriter w;
    std::vector<std::uint8_t> raw = {0x00, 0x01, 0x00, 0xFF, 0x00};
    w.writeBytes(raw);
    BinaryReader r(w.data().data(), w.size());
    auto back = r.readBytes();
    LSX_EXPECT_TRUE(back == raw);
}

// ---------- TLV ----------

LSX_TEST(Serialize, TLVRoundtrip) {
    BinaryWriter w;
    w.writeField(1, std::uint64_t(123));               // varint 字段
    w.writeField(2, std::string("tlv string"));        // 长度前缀字段
    std::vector<std::uint8_t> payload = {0xCA, 0xFE};
    w.writeField(3, WireType::LengthDelimited, payload);

    BinaryReader r(w.data().data(), w.size());
    auto f1 = r.readField();
    LSX_EXPECT_EQ(f1.tag, 1u);
    LSX_EXPECT_EQ(f1.type, WireType::Varint);
    LSX_EXPECT_EQ(f1.varint, std::uint64_t(123));

    auto f2 = r.readField();
    LSX_EXPECT_EQ(f2.tag, 2u);
    LSX_EXPECT_EQ(f2.type, WireType::LengthDelimited);
    std::string s(f2.bytes.begin(), f2.bytes.end());
    LSX_EXPECT_EQ(s, std::string("tlv string"));

    auto f3 = r.readField();
    LSX_EXPECT_EQ(f3.tag, 3u);
    LSX_EXPECT_TRUE(f3.bytes == payload);
    LSX_EXPECT_TRUE(!r.fail());
}

LSX_TEST(Serialize, TLVFieldOrderPreserved) {
    BinaryWriter w;
    w.writeField(5, std::uint64_t(1));
    w.writeField(5, std::uint64_t(2));   // 同 tag 重复出现（repeated 语义）
    w.writeField(9, std::uint64_t(3));

    BinaryReader r(w.data().data(), w.size());
    LSX_EXPECT_EQ(r.readField().varint, std::uint64_t(1));
    LSX_EXPECT_EQ(r.readField().varint, std::uint64_t(2));
    auto f = r.readField();
    LSX_EXPECT_EQ(f.tag, 9u);
}

// ---------- 损坏/截断输入 ----------

LSX_TEST(Serialize, TruncatedReadFailsGracefully) {
    BinaryWriter w;
    w.writeU32(0x12345678);
    std::vector<std::uint8_t> truncated(w.data().begin(), w.data().begin() + 2);
    BinaryReader r(truncated.data(), truncated.size());
    LSX_EXPECT_EQ(r.readU32(), std::uint32_t(0));   // 越界读返回 0
    LSX_EXPECT_TRUE(r.fail());
    LSX_EXPECT_EQ(r.error(), ReadError::OutOfBounds);
    LSX_EXPECT_EQ(r.readU32(), std::uint32_t(0));   // 失败后短路，继续返回 0
}

LSX_TEST(Serialize, TruncatedStringFailsGracefully) {
    BinaryWriter w;
    w.writeString("hello world");
    // 截掉一半
    std::vector<std::uint8_t> cut(w.data().begin(), w.data().begin() + 4);
    BinaryReader r(cut.data(), cut.size());
    auto s = r.readString();
    LSX_EXPECT_TRUE(r.fail());       // 长度声称 11 字节但只剩 2
    LSX_EXPECT_TRUE(s.empty());
}

LSX_TEST(Serialize, VarintOverflowFailsGracefully) {
    // 11 个连续续传字节 → varint 超限
    std::uint8_t bad[11];
    for (int i = 0; i < 11; ++i) bad[i] = 0x80;
    BinaryReader r(bad, sizeof(bad));
    r.readVarint();
    LSX_EXPECT_TRUE(r.fail());
    LSX_EXPECT_EQ(r.error(), ReadError::LengthOverflow);
}

LSX_TEST(Serialize, UnknownWireTypeFailsGracefully) {
    // tag<<3 | 6 → wire type 6 未定义
    std::uint8_t bad[] = {0x06};     // key = 6 → tag 0, wiretype 6
    BinaryReader r(bad, sizeof(bad));
    r.readField();
    LSX_EXPECT_TRUE(r.fail());
    LSX_EXPECT_EQ(r.error(), ReadError::BadWireType);
}

LSX_TEST(Serialize, TLVLengthBeyondBufferFails) {
    // key = tag1, wiretype2；长度 = 100；实际只有 0 字节载荷
    std::uint8_t bad[] = {0x0A, 0x64};
    BinaryReader r(bad, sizeof(bad));
    r.readField();
    LSX_EXPECT_TRUE(r.fail());
    LSX_EXPECT_EQ(r.error(), ReadError::OutOfBounds);
}

LSX_TEST(Serialize, EmptyBufferReadsFailGracefully) {
    BinaryReader r(nullptr, 0);
    LSX_EXPECT_EQ(r.readU8(), std::uint8_t(0));
    LSX_EXPECT_EQ(r.readVarint(), std::uint64_t(0));
    LSX_EXPECT_TRUE(r.readString().empty());
    LSX_EXPECT_TRUE(r.fail());
}

// ---------- 消息级协议模式 ----------

LSX_TEST(Serialize, MessageRoundtripProtocol) {
    // 典型设备消息：header(varint msg_type) + TLV 字段若干 + checksum(U32)
    struct SensorMsg { std::uint32_t device_id; float value; std::string unit; };
    SensorMsg out{42, 25.5f, "°C"};

    BinaryWriter w;
    w.writeVarint(0x1001);                     // 消息类型
    w.writeField(1, std::uint64_t(out.device_id));
    w.writeField(2, WireType::Fixed32,
                 reinterpret_cast<const std::uint8_t*>(&out.value), 4);
    w.writeField(3, out.unit);

    BinaryReader r(w.data().data(), w.size());
    LSX_EXPECT_EQ(r.readVarint(), std::uint64_t(0x1001));
    SensorMsg in{};
    while (r.remaining() > 0 && !r.fail())
    {
        auto f = r.readField();
        if (r.fail()) break;
        switch (f.tag)
        {
        case 1: in.device_id = static_cast<std::uint32_t>(f.varint); break;
        case 2:
        {
            // Fixed32 载荷存于 f.varint 低 32 位
            std::uint32_t bits = static_cast<std::uint32_t>(f.varint);
            float v;
            std::memcpy(&v, &bits, sizeof(v));
            in.value = v;
            break;
        }
        case 3: in.unit.assign(f.bytes.begin(), f.bytes.end()); break;
        default: break;   // 未知字段跳过（TLV 语义天然支持）
        }
    }
    LSX_EXPECT_TRUE(!r.fail());
    LSX_EXPECT_EQ(in.device_id, 42u);
    LSX_EXPECT_TRUE(std::fabs(in.value - 25.5f) < 1e-5f);
    LSX_EXPECT_EQ(in.unit, std::string("°C"));
}

LSX_TEST(Serialize, UnknownFieldSkipped) {
    BinaryWriter w;
    w.writeField(1, std::uint64_t(100));          // 已知
    w.writeField(99, std::string("future field")); // 未知（新版本写入）
    w.writeField(2, std::uint64_t(200));          // 已知

    BinaryReader r(w.data().data(), w.size());
    LSX_EXPECT_EQ(r.readField().varint, std::uint64_t(100));
    auto unknown = r.readField();                  // 旧版本读到未知字段
    LSX_EXPECT_EQ(unknown.tag, 99u);               // TLV 语义可完整跳过
    LSX_EXPECT_EQ(r.readField().varint, std::uint64_t(200));   // 后续字段不受影响
    LSX_EXPECT_TRUE(!r.fail());
}

// ---------- Writer 容量与复用 ----------

LSX_TEST(Serialize, WriterClearReuses) {
    BinaryWriter w(4096);   // 预留
    for (int i = 0; i < 100; ++i) w.writeU32(i);
    LSX_EXPECT_EQ(w.size(), 400u);
    w.clear();
    LSX_EXPECT_EQ(w.size(), 0u);
    w.writeU8(1);
    LSX_EXPECT_EQ(w.size(), 1u);
}

LSX_TEST(Serialize, LargePayloadRoundtrip) {
    BinaryWriter w;
    std::string big(64 * 1024, 'z');    // 64KB
    w.writeString(big);
    BinaryReader r(w.data().data(), w.size());
    LSX_EXPECT_EQ(r.readString(), big);
    LSX_EXPECT_TRUE(!r.fail());
}
