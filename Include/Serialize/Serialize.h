/**
 * @file Serialize.h
 * @brief LIBLSX 工具库 - 二进制序列化模块
 * @details 提供 LSX_LIB::Serialize 命名空间下的轻量二进制编解码工具，
 *          面向嵌入式设备间通信协议（配合 DataTransfer 使用）：
 *  - BinaryWriter：写入固定宽度整数（小端）、varint、字符串、字节数组、TLV 字段
 *  - BinaryReader：对称读取；读取失败置 fail 标志而不抛异常，适合解析不可信输入
 *
 * ### 编码约定
 *  - 多字节整数一律**小端**（与 x86/ARM 内存布局一致，跨平台传输需统一约定）
 *  - varint：LEB128 编码，负数请使用 zigzag 辅助（writeVarintZZ/readVarintZZ）
 *  - 字符串：varint 长度前缀 + 原始字节（不含结尾 NUL）
 *  - TLV：writeField(tag, ...) = varint(tag<<3 | wire_type) + payload，
 *    兼容 protobuf wire format 的字段跳过语义（未知字段可跳过）
 *
 * @author 连思鑫（jinbilianshao）
 * @date 2026-11-10
 * @version 1.0
 *
 * ### 使用示例
 * @code
 * #include "Serialize/Serialize.h"
 *
 * LSX_LIB::Serialize::BinaryWriter w;
 * w.writeVarint(42);
 * w.writeString("hello");
 * w.writeField(1, static_cast<std::uint32_t>(7));   // TLV 字段 tag=1
 *
 * LSX_LIB::Serialize::BinaryReader r(w.data(), w.size());
 * auto v = r.readVarint();            // 42
 * auto s = r.readString();            // "hello"
 * auto [tag, val] = r.readField();    // tag=1, val=7
 * @endcode
 */
#pragma once

#include <cstdint>
#include <cstddef>
#include <limits>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace LSX_LIB {
namespace Serialize {

/// TLV 线路类型（对齐 protobuf wire format，便于互操作与未知字段跳过）
enum class WireType : std::uint8_t
{
    Varint = 0,          ///< varint 编码整数
    Fixed64 = 1,         ///< 8 字节定长（uint64/double）
    LengthDelimited = 2, ///< varint 长度前缀的字节串
    Fixed32 = 5          ///< 4 字节定长（uint32/float）
};

/// 解析失败的具体原因
enum class ReadError : std::uint8_t
{
    None = 0,
    OutOfBounds,     ///< 读取超出缓冲区
    LengthOverflow,  ///< varint 长度超限（>64 位或 > 预留上限）
    BadWireType,     ///< TLV 字段线路类型未知
    FieldTooLarge    ///< TLV 长度前缀超过缓冲区剩余
};

/**
 * @class BinaryWriter
 * @brief 小端二进制写入器
 * @details 内部持有 std::vector<uint8_t> 缓冲区，写入失败（如 TLV 超上限）
 * 置 fail 标志；写入永不抛异常。
 */
class BinaryWriter
{
public:
    /// 预留容量（减少扩容）
    explicit BinaryWriter(std::size_t reserve = 256)
    {
        buf_.reserve(reserve);
    }

    // ---- 固定宽度整数（小端） ----

    void writeU8(std::uint8_t v)  { buf_.push_back(v); }

    void writeU16(std::uint16_t v)
    {
        buf_.push_back(static_cast<std::uint8_t>(v));
        buf_.push_back(static_cast<std::uint8_t>(v >> 8));
    }

    void writeU32(std::uint32_t v)
    {
        for (int i = 0; i < 4; ++i)
        {
            buf_.push_back(static_cast<std::uint8_t>(v >> (i * 8)));
        }
    }

    void writeU64(std::uint64_t v)
    {
        for (int i = 0; i < 8; ++i)
        {
            buf_.push_back(static_cast<std::uint8_t>(v >> (i * 8)));
        }
    }

    void writeI8(std::int8_t v)   { writeU8(static_cast<std::uint8_t>(v)); }
    void writeI16(std::int16_t v) { writeU16(static_cast<std::uint16_t>(v)); }
    void writeI32(std::int32_t v) { writeU32(static_cast<std::uint32_t>(v)); }
    void writeI64(std::int64_t v) { writeU64(static_cast<std::uint64_t>(v)); }

    void writeF32(float v)
    {
        std::uint32_t bits;
        std::memcpy(&bits, &v, sizeof(bits));
        writeU32(bits);
    }

    void writeF64(double v)
    {
        std::uint64_t bits;
        std::memcpy(&bits, &v, sizeof(bits));
        writeU64(bits);
    }

    void writeBool(bool v) { writeU8(v ? 1 : 0); }

    // ---- varint（LEB128） ----

    /// 无符号 varint（LEB128，每字节低 7 位数据 + 最高位续传标志）
    void writeVarint(std::uint64_t v)
    {
        while (v >= 0x80)
        {
            buf_.push_back(static_cast<std::uint8_t>(v) | 0x80);
            v >>= 7;
        }
        buf_.push_back(static_cast<std::uint8_t>(v));
    }

    /// 有符号数 zigzag 编码 varint（-1→1, 1→2, -2→3...，小绝对值更省字节）
    void writeVarintZZ(std::int64_t v)
    {
        std::uint64_t zz = (static_cast<std::uint64_t>(v) << 1) ^
                           static_cast<std::uint64_t>(v >> 63);
        writeVarint(zz);
    }

    // ---- 字符串与字节数组 ----

    /// varint 长度前缀 + 原始字节（无 NUL 结尾）
    void writeString(const std::string& s)
    {
        writeVarint(s.size());
        buf_.insert(buf_.end(), s.begin(), s.end());
    }

    void writeBytes(const std::uint8_t* data, std::size_t len)
    {
        writeVarint(len);
        buf_.insert(buf_.end(), data, data + len);
    }

    void writeBytes(const std::vector<std::uint8_t>& v)
    {
        writeBytes(v.data(), v.size());
    }

    // ---- TLV 字段（对齐 protobuf wire format） ----

    /// @brief 写入一个 TLV 字段：varint(tag<<3 | wire_type) + payload
    /// @note LengthDelimited 载荷带 varint 长度前缀；Fixed32/Fixed64 为定长无前缀
    void writeField(std::uint32_t tag, WireType type, const std::uint8_t* payload,
                    std::size_t len)
    {
        writeVarint((static_cast<std::uint64_t>(tag) << 3) |
                    static_cast<std::uint64_t>(type));
        if (type == WireType::LengthDelimited)
        {
            writeBytes(payload, len);   // 长度前缀 + 数据
        }
        else
        {
            // Fixed32/Fixed64：定长载荷，无长度前缀（对齐 protobuf wire format）
            buf_.insert(buf_.end(), payload, payload + len);
        }
    }

    void writeField(std::uint32_t tag, WireType type,
                    const std::vector<std::uint8_t>& payload)
    {
        writeField(tag, type, payload.data(), payload.size());
    }

    void writeField(std::uint32_t tag, std::uint64_t varint_value)
    {
        writeVarint((static_cast<std::uint64_t>(tag) << 3) |
                    static_cast<std::uint64_t>(WireType::Varint));
        writeVarint(varint_value);
    }

    void writeField(std::uint32_t tag, std::int64_t varint_value)
    {
        writeVarint((static_cast<std::uint64_t>(tag) << 3) |
                    static_cast<std::uint64_t>(WireType::Varint));
        writeVarintZZ(varint_value);
    }

    void writeField(std::uint32_t tag, const std::string& s)
    {
        writeField(tag, WireType::LengthDelimited,
                   reinterpret_cast<const std::uint8_t*>(s.data()), s.size());
    }

    // ---- 缓冲区访问 ----

    const std::vector<std::uint8_t>& data() const { return buf_; }
    std::size_t size() const { return buf_.size(); }
    void clear() { buf_.clear(); }

private:
    std::vector<std::uint8_t> buf_;
};

/**
 * @class BinaryReader
 * @brief 二进制读取器：与 BinaryWriter 对称；失败置 fail 标志不抛异常
 * @details 面向解析不可信输入（网络对端）：任何越界/格式错误只置 fail，
 * 后续读取返回零值。调用方只需最后检查 fail()/error()。
 */
class BinaryReader
{
public:
    BinaryReader(const std::uint8_t* data, std::size_t size)
        : data_(data), size_(size) {}

    explicit BinaryReader(const std::vector<std::uint8_t>& v)
        : data_(v.data()), size_(v.size()) {}

    // ---- 状态 ----

    /// 是否发生读取错误
    bool fail() const { return err_ != ReadError::None; }
    ReadError error() const { return err_; }
    /// 当前读取偏移
    std::size_t pos() const { return pos_; }
    /// 剩余可读字节数
    std::size_t remaining() const { return size_ - pos_; }
    /// 跳过 n 字节（越界置 fail）
    void skip(std::size_t n)
    {
        if (pos_ + n > size_)
        {
            err_ = ReadError::OutOfBounds;
            return;
        }
        pos_ += n;
    }

    // ---- 固定宽度整数（小端） ----

    std::uint8_t readU8()
    {
        if (!check(1)) return 0;
        return data_[pos_++];
    }

    std::uint16_t readU16()
    {
        if (!check(2)) return 0;
        std::uint16_t v = static_cast<std::uint16_t>(data_[pos_]) |
                          static_cast<std::uint16_t>(data_[pos_ + 1]) << 8;
        pos_ += 2;
        return v;
    }

    std::uint32_t readU32()
    {
        if (!check(4)) return 0;
        std::uint32_t v = 0;
        for (int i = 0; i < 4; ++i)
        {
            v |= static_cast<std::uint32_t>(data_[pos_ + i]) << (i * 8);
        }
        pos_ += 4;
        return v;
    }

    std::uint64_t readU64()
    {
        if (!check(8)) return 0;
        std::uint64_t v = 0;
        for (int i = 0; i < 8; ++i)
        {
            v |= static_cast<std::uint64_t>(data_[pos_ + i]) << (i * 8);
        }
        pos_ += 8;
        return v;
    }

    std::int8_t  readI8()  { return static_cast<std::int8_t>(readU8()); }
    std::int16_t readI16() { return static_cast<std::int16_t>(readU16()); }
    std::int32_t readI32() { return static_cast<std::int32_t>(readU32()); }
    std::int64_t readI64() { return static_cast<std::int64_t>(readU64()); }

    float readF32()
    {
        std::uint32_t bits = readU32();
        float v;
        std::memcpy(&v, &bits, sizeof(v));
        return v;
    }

    double readF64()
    {
        std::uint64_t bits = readU64();
        double v;
        std::memcpy(&v, &bits, sizeof(v));
        return v;
    }

    bool readBool()
    {
        std::uint8_t v = readU8();
        return v != 0;
    }

    // ---- varint（LEB128） ----

    /// 读取无符号 varint；最多消费 10 字节，超限置 LengthOverflow
    std::uint64_t readVarint()
    {
        std::uint64_t v = 0;
        int shift = 0;
        for (int i = 0; i < 10; ++i)   // 64 位 varint 最多 10 字节
        {
            if (!check(1)) return 0;
            std::uint8_t b = data_[pos_++];
            v |= static_cast<std::uint64_t>(b & 0x7F) << shift;
            if ((b & 0x80) == 0)
            {
                return v;
            }
            shift += 7;
        }
        err_ = ReadError::LengthOverflow;  // 连续 10 字节仍有续传位
        return 0;
    }

    /// 读取 zigzag 编码的 varint，还原为有符号数
    std::int64_t readVarintZZ()
    {
        std::uint64_t zz = readVarint();
        return static_cast<std::int64_t>((zz >> 1) ^ (~(zz & 1) + 1));
    }

    // ---- 字符串与字节数组 ----

    std::string readString()
    {
        std::uint64_t len = readVarint();
        // arm32 上 size_t 为 32 位：先判断 64 位长度是否可无损转换，
        // 否则 0x1_0000_0000 会截断成 0 并绕过 check() 越界检查。
        if (len > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) { err_ = ReadError::LengthOverflow; return {}; }
        if (fail() || !check(static_cast<std::size_t>(len))) return {};
        std::string s(reinterpret_cast<const char*>(data_ + pos_),
                      static_cast<std::size_t>(len));
        pos_ += static_cast<std::size_t>(len);
        return s;
    }

    /// 读取 varint 长度前缀 + 字节数组（与 writeBytes 对称）
    std::vector<std::uint8_t> readBytes()
    {
        std::uint64_t len = readVarint();
        if (len > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) { err_ = ReadError::LengthOverflow; return {}; }
        if (fail() || !check(static_cast<std::size_t>(len))) return {};
        std::vector<std::uint8_t> v(data_ + pos_, data_ + pos_ + static_cast<std::size_t>(len));
        pos_ += static_cast<std::size_t>(len);
        return v;
    }

    // ---- TLV 字段 ----

    /// TLV 字段读取结果
    struct Field
    {
        std::uint32_t tag = 0;
        WireType type = WireType::Varint;
        std::uint64_t varint = 0;                      ///< Varint 类型载荷
        std::vector<std::uint8_t> bytes;               ///< LengthDelimited 载荷
    };

    /// @brief 读取一个 TLV 字段（对齐 protobuf wire format）
    Field readField()
    {
        Field f;
        std::uint64_t key = readVarint();
        if (fail())
        {
            err_ = ReadError::OutOfBounds;
            return f;
        }
        f.tag = static_cast<std::uint32_t>(key >> 3);
        auto wt = static_cast<WireType>(key & 0x07);
        f.type = wt;
        switch (wt)
        {
        case WireType::Varint:
            f.varint = readVarint();
            break;
        case WireType::Fixed64:
            if (check(8)) { f.varint = readU64(); }
            break;
        case WireType::Fixed32:
            if (check(4)) { f.varint = readU32(); }
            break;
        case WireType::LengthDelimited:
        {
            std::uint64_t len = readVarint();
            if (!fail() && check(static_cast<std::size_t>(len)))
            {
                f.bytes.assign(data_ + pos_, data_ + pos_ + len);
                pos_ += static_cast<std::size_t>(len);
            }
            break;
        }
        default:
            err_ = ReadError::BadWireType;   // wire type 3/4/6/7 未定义
            break;
        }
        return f;
    }

private:
    /// 确认还有 n 字节可读；不足置 OutOfBounds
    bool check(std::size_t n)
    {
        if (err_ != ReadError::None)
        {
            return false;   // 已失败：后续读取一律短路
        }
        if (size_ - pos_ < n)
        {
            err_ = ReadError::OutOfBounds;
            return false;
        }
        return true;
    }

    const std::uint8_t* data_;
    std::size_t size_;
    std::size_t pos_ = 0;
    ReadError err_ = ReadError::None;
};

} // namespace Serialize
} // namespace LSX_LIB
