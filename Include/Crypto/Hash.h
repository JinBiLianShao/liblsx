/**
 * @file Hash.h
 * @brief LIBLSX 工具库 - 摘要、MAC 与编码（纯实现，无第三方依赖）
 * @details 提供 LSX_LIB::Crypto 命名空间下的：
 *  - sha256 / sha256Hex：FIPS 180-4 SHA-256
 *  - hmacSha256：RFC 2104 HMAC-SHA256
 *  - base64Encode / base64Decode：RFC 4648
 *  - hexEncode / hexDecode：十六进制编解码
 *  - 常量时间比较（防时序侧信道）
 *
 * @author 连思鑫（jinbilianshao）
 * @date 2026-11-11
 * @version 1.0
 */
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace LSX_LIB {
namespace Crypto {

/// SHA-256 摘要（32 字节）
std::array<std::uint8_t, 32> sha256(const std::uint8_t* data, std::size_t len);
inline std::array<std::uint8_t, 32> sha256(const std::vector<std::uint8_t>& d)
{
    return sha256(d.data(), d.size());
}
inline std::array<std::uint8_t, 32> sha256(const std::string& s)
{
    return sha256(reinterpret_cast<const std::uint8_t*>(s.data()), s.size());
}

/// SHA-256 十六进制字符串（小写，64 字符）
std::string sha256Hex(const std::uint8_t* data, std::size_t len);
inline std::string sha256Hex(const std::string& s)
{
    return sha256Hex(reinterpret_cast<const std::uint8_t*>(s.data()), s.size());
}

/// HMAC-SHA256（RFC 2104）
std::array<std::uint8_t, 32> hmacSha256(const std::uint8_t* key, std::size_t key_len,
                                        const std::uint8_t* data, std::size_t data_len);
inline std::array<std::uint8_t, 32> hmacSha256(const std::string& key,
                                               const std::string& data)
{
    return hmacSha256(reinterpret_cast<const std::uint8_t*>(key.data()), key.size(),
                      reinterpret_cast<const std::uint8_t*>(data.data()), data.size());
}
std::string hmacSha256Hex(const std::string& key, const std::string& data);

/// Base64（RFC 4648）
std::string base64Encode(const std::uint8_t* data, std::size_t len);
inline std::string base64Encode(const std::string& s)
{
    return base64Encode(reinterpret_cast<const std::uint8_t*>(s.data()), s.size());
}
/// 返回空表示输入非法（长度/字符错误）
std::vector<std::uint8_t> base64Decode(const std::string& text);

/// 十六进制（小写输出；解码大小写均可）
std::string hexEncode(const std::uint8_t* data, std::size_t len);
inline std::string hexEncode(const std::vector<std::uint8_t>& d)
{
    return hexEncode(d.data(), d.size());
}
std::vector<std::uint8_t> hexDecode(const std::string& text);

/// 常量时间字节比较（长度不同立即返回 false）
bool constantTimeEquals(const std::uint8_t* a, std::size_t an,
                        const std::uint8_t* b, std::size_t bn);

} // namespace Crypto
} // namespace LSX_LIB
