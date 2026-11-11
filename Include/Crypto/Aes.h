/**
 * @file Aes.h
 * @brief LIBLSX 工具库 - AES 对称加密 C++ 封装
 * @details 基于 vendor 的 tiny-AES-c（Unlicense），提供 AES-128/192/256 的
 *  ECB / CBC / CTR 模式。密钥长度运行时选择（内部三套实例按密钥长度分发）。
 *  CBC 模式内置 PKCS#7 填充，输入可为任意长度。
 *
 * @warning ECB 模式不具备语义安全性，仅用于兼容旧协议；新设计请用 CBC/CTR。
 *          CBC/CTR 的 IV 对同一密钥绝不可重复使用。
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
namespace Crypto {

enum class AesMode : std::uint8_t
{
    ECB,  ///< 电子密码本（不安全，仅兼容用）
    CBC,  ///< 密码块链接（需 16 字节 IV，自动 PKCS#7 填充）
    CTR   ///< 计数器模式（流加密，无需填充，需 16 字节 IV/计数器初值）
};

/**
 * @class Aes
 * @brief AES 加解密器
 *
 * ### 使用示例
 * @code
 * #include "Crypto/Aes.h"
 *
 * std::vector<std::uint8_t> key(16, 0x2B);  // AES-128
 * std::vector<std::uint8_t> iv(16, 0x00);
 *
 * LSX_LIB::Crypto::Aes aes(key);
 * auto cipher = aes.encrypt(plaintext, LSX_LIB::Crypto::AesMode::CBC, iv);
 * auto plain  = aes.decrypt(cipher,   LSX_LIB::Crypto::AesMode::CBC, iv);
 * @endcode
 */
class Aes
{
public:
    /// 构造：key 长度必须为 16/24/32 字节，否则所有操作返回空
    explicit Aes(const std::vector<std::uint8_t>& key);
    explicit Aes(const std::string& key);

    bool valid() const { return key_bits_ != 0; }

    /// ECB：data 长度必须是 16 的倍数（不做填充）
    std::vector<std::uint8_t> ecbEncrypt(const std::vector<std::uint8_t>& data) const;
    std::vector<std::uint8_t> ecbDecrypt(const std::vector<std::uint8_t>& data) const;

    /// CBC：自动 PKCS#7 填充；iv 必须 16 字节
    std::vector<std::uint8_t> cbcEncrypt(const std::vector<std::uint8_t>& data,
                                        const std::vector<std::uint8_t>& iv) const;
    std::vector<std::uint8_t> cbcDecrypt(const std::vector<std::uint8_t>& data,
                                        const std::vector<std::uint8_t>& iv) const;

    /// CTR：流加密，任意长度；iv 作为初始计数器，必须 16 字节
    std::vector<std::uint8_t> ctrXcrypt(const std::vector<std::uint8_t>& data,
                                       const std::vector<std::uint8_t>& iv) const;

    /// 统一入口
    std::vector<std::uint8_t> encrypt(const std::vector<std::uint8_t>& data, AesMode mode,
                                      const std::vector<std::uint8_t>& iv = {}) const;
    std::vector<std::uint8_t> decrypt(const std::vector<std::uint8_t>& data, AesMode mode,
                                      const std::vector<std::uint8_t>& iv = {}) const;

private:
    std::vector<std::uint8_t> key_;
    int key_bits_ = 0;   // 128/192/256，0 = 无效

    void cryptEcb(std::uint8_t* buf, std::size_t len, bool encrypt) const;
    void cryptCbc(std::uint8_t* buf, std::size_t len,
                  const std::uint8_t* iv, bool encrypt) const;
    void cryptCtr(std::uint8_t* buf, std::size_t len, const std::uint8_t* iv) const;
};

/// PKCS#7 填充 / 去填充（供需要自行控制填充的场景）
std::vector<std::uint8_t> pkcs7Pad(const std::vector<std::uint8_t>& data,
                                   std::size_t block = 16);
bool pkcs7Unpad(std::vector<std::uint8_t>& data, std::size_t block = 16);

} // namespace Crypto
} // namespace LSX_LIB
