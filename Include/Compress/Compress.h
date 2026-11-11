/**
 * @file Compress.h
 * @brief LIBLSX 工具库 - 压缩模块
 * @details 提供 LSX_LIB::Compress 命名空间下的压缩能力：
 *  - deflate / inflate：基于 vendor 的 miniz 3.1.2（MIT），RFC 1951 原始 DEFLATE
 *  - zlibCompress / zlibDecompress：RFC 1950 zlib 封装（带 Adler-32 校验头）
 *  - gzipCompress / gzipDecompress：RFC 1952 gzip 封装
 *  - rleEncode / rleDecode：自实现的游程编码（MCU 上比 DEFLATE 轻，适合传感器连续重复数据）
 *  - crc32 / adler32：直接复用 miniz 的实现
 *
 * 全部接口返回 std::vector<uint8_t>，失败返回空 vector（不抛异常）。
 *
 * @author 连思鑫（jinbilianshao）
 * @date 2026-11-11
 * @version 1.0
 */
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace LSX_LIB {
namespace Compress {

/// 压缩级别 0（仅存储）~ 10（最大压缩）
enum class Level : int
{
    NoCompression = 0,
    Fastest = 1,
    Default = 6,
    Best = 10
};

// ---------- DEFLATE（RFC 1951 原始流） ----------

std::vector<std::uint8_t> deflate(const std::uint8_t* data, std::size_t len,
                                  Level level = Level::Default);
inline std::vector<std::uint8_t> deflate(const std::vector<std::uint8_t>& d,
                                         Level level = Level::Default)
{
    return deflate(d.data(), d.size(), level);
}
inline std::vector<std::uint8_t> deflate(const std::string& s,
                                         Level level = Level::Default)
{
    return deflate(reinterpret_cast<const std::uint8_t*>(s.data()), s.size(), level);
}

/// @param expected_size 期望的解压后大小（0 = 自动按 2 倍增量扩张，上限 64MB）
std::vector<std::uint8_t> inflate(const std::uint8_t* data, std::size_t len,
                                  std::size_t expected_size = 0);
inline std::vector<std::uint8_t> inflate(const std::vector<std::uint8_t>& d,
                                         std::size_t expected_size = 0)
{
    return inflate(d.data(), d.size(), expected_size);
}

// ---------- zlib 封装（RFC 1950，带 2 字节头 + 4 字节 Adler-32 尾） ----------

std::vector<std::uint8_t> zlibCompress(const std::vector<std::uint8_t>& data,
                                       Level level = Level::Default);
std::vector<std::uint8_t> zlibDecompress(const std::vector<std::uint8_t>& data);

// ---------- gzip 封装（RFC 1952） ----------

std::vector<std::uint8_t> gzipCompress(const std::vector<std::uint8_t>& data,
                                       Level level = Level::Default);
std::vector<std::uint8_t> gzipDecompress(const std::vector<std::uint8_t>& data);

// ---------- 轻量 RLE（自实现） ----------

/**
 * @brief 游程编码：连续相同字节压成 [value, count] 对（count 1..255）
 * @details 对不重复数据会膨胀（最差 2 倍），仅适合重复度高的场景（如 ADC 采样、位图）。
 */
std::vector<std::uint8_t> rleEncode(const std::uint8_t* data, std::size_t len);
inline std::vector<std::uint8_t> rleEncode(const std::vector<std::uint8_t>& d)
{
    return rleEncode(d.data(), d.size());
}
/// 解码：输入必须是偶数长度且 count 非 0，否则返回空
std::vector<std::uint8_t> rleDecode(const std::uint8_t* data, std::size_t len);
inline std::vector<std::uint8_t> rleDecode(const std::vector<std::uint8_t>& d)
{
    return rleDecode(d.data(), d.size());
}

// ---------- 校验和（复用 miniz 实现） ----------

std::uint32_t crc32(const std::uint8_t* data, std::size_t len);
std::uint32_t adler32(const std::uint8_t* data, std::size_t len);

} // namespace Compress
} // namespace LSX_LIB
