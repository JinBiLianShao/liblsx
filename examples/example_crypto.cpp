/**
 * @file example_crypto.cpp
 * @brief LIBLSX Crypto 模块使用示例
 *
 * 演示内容：
 *   1. AES 加解密（CBC，自动 PKCS#7 填充）
 *   2. SHA-256 摘要与 HMAC-SHA256
 *   3. Base64 / Hex 编解码
 *   4. 常量时间比较
 *
 * 关键点：
 *   - AES key 长度决定档位：16/24/32 字节 -> AES-128/192/256，非法长度 valid()==false。
 *   - CBC 自动 PKCS#7 填充（输入任意长度）；ECB 要求 16 倍数且不做填充。
 *   - IV 对同一密钥绝不可重复使用（CBC/CTR）。
 *   - 所有加密接口失败返回空 vector，不抛异常。
 */

#include "Crypto/lsx_crypto.h"

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace cr = LSX_LIB::Crypto;

int main()
{
    std::cout << "===== Crypto 加解密/摘要/编码示例 =====\n";

    // ---- 1) AES-CBC 加解密（自动 PKCS#7 填充）----
    std::vector<std::uint8_t> key(16, 0x2B);   // 16 字节 -> AES-128
    std::vector<std::uint8_t> iv(16, 0x00);    // 16 字节 IV
    cr::Aes aes(key);
    std::cout << "  AES 有效: " << (aes.valid() ? "是" : "否") << "\n";

    // 明文 5 字节（非 16 倍数，靠 PKCS#7 自动填充）
    std::vector<std::uint8_t> plain{0x68, 0x65, 0x6C, 0x6C, 0x6F};  // "hello"
    auto cipher = aes.encrypt(plain, cr::AesMode::CBC, iv);
    auto back = aes.decrypt(cipher, cr::AesMode::CBC, iv);
    std::cout << "  CBC: 明文 " << plain.size() << " 字节 -> 密文 "
              << cipher.size() << " 字节 -> 还原 " << back.size() << " 字节\n";

    // ---- 2) SHA-256 与 HMAC-SHA256（十六进制输出）----
    std::cout << "  sha256(abc)        = " << cr::sha256Hex(std::string("abc")) << "\n";
    std::cout << "  hmac-sha256(key,data) = "
              << cr::hmacSha256Hex("key", "The quick brown fox") << "\n";

    // ---- 3) Base64 / Hex 编解码 ----
    std::string b64 = cr::base64Encode(std::string("hello"));
    auto decoded = cr::base64Decode(b64);
    std::cout << "  base64(hello) = " << b64 << "，解码回 " << decoded.size() << " 字节\n";

    std::string hex = cr::hexEncode(reinterpret_cast<const std::uint8_t*>("AB"), 2);
    std::cout << "  hex(AB) = " << hex << "\n";

    // ---- 4) 常量时间比较（防时序侧信道）----
    std::uint8_t a[] = {1, 2, 3}, b[] = {1, 2, 3}, c[] = {1, 2, 4};
    std::cout << "  常量时间比较: 相同=" << cr::constantTimeEquals(a, 3, b, 3)
              << "，不同=" << cr::constantTimeEquals(a, 3, c, 3) << "\n";

    return 0;
}
