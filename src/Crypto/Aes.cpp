/**
 * @file Aes.cpp
 * @brief AES C++ 封装实现（分发到 vendor 的 128/192/256 三实例）
 */
#include "Crypto/Aes.h"

#include <algorithm>
#include <cstring>

// 三个实例的符号声明（由 aes_variants.cpp 提供）。
// 不 include vendor 的 aes.h：其 AES_ctx 布局随密钥长度变化，这里用不透明存储区 + 显式大小常量。
// 布局（见 vendor aes.h）：RoundKey[AES_keyExpSize] + Iv[16]；
// AES_keyExpSize = 176/208/240 对应 128/192/256。
constexpr std::size_t kCtx128 = 176 + 16;
constexpr std::size_t kCtx192 = 208 + 16;
constexpr std::size_t kCtx256 = 240 + 16;

namespace lsx_aes128 {
struct AES_ctx;
void AES_init_ctx(struct AES_ctx*, const std::uint8_t*);
void AES_init_ctx_iv(struct AES_ctx*, const std::uint8_t*, const std::uint8_t*);
void AES_ECB_encrypt(const struct AES_ctx*, std::uint8_t*);
void AES_ECB_decrypt(const struct AES_ctx*, std::uint8_t*);
void AES_CBC_encrypt_buffer(struct AES_ctx*, std::uint8_t*, std::size_t);
void AES_CBC_decrypt_buffer(struct AES_ctx*, std::uint8_t*, std::size_t);
void AES_CTR_xcrypt_buffer(struct AES_ctx*, std::uint8_t*, std::size_t);
}
namespace lsx_aes192 {
struct AES_ctx;
void AES_init_ctx(struct AES_ctx*, const std::uint8_t*);
void AES_init_ctx_iv(struct AES_ctx*, const std::uint8_t*, const std::uint8_t*);
void AES_ECB_encrypt(const struct AES_ctx*, std::uint8_t*);
void AES_ECB_decrypt(const struct AES_ctx*, std::uint8_t*);
void AES_CBC_encrypt_buffer(struct AES_ctx*, std::uint8_t*, std::size_t);
void AES_CBC_decrypt_buffer(struct AES_ctx*, std::uint8_t*, std::size_t);
void AES_CTR_xcrypt_buffer(struct AES_ctx*, std::uint8_t*, std::size_t);
}
namespace lsx_aes256 {
struct AES_ctx;
void AES_init_ctx(struct AES_ctx*, const std::uint8_t*);
void AES_init_ctx_iv(struct AES_ctx*, const std::uint8_t*, const std::uint8_t*);
void AES_ECB_encrypt(const struct AES_ctx*, std::uint8_t*);
void AES_ECB_decrypt(const struct AES_ctx*, std::uint8_t*);
void AES_CBC_encrypt_buffer(struct AES_ctx*, std::uint8_t*, std::size_t);
void AES_CBC_decrypt_buffer(struct AES_ctx*, std::uint8_t*, std::size_t);
void AES_CTR_xcrypt_buffer(struct AES_ctx*, std::uint8_t*, std::size_t);
}

namespace LSX_LIB {
namespace Crypto {

namespace
{
constexpr std::size_t kBlock = 16;

// 容纳最大实例（AES-256：240+16=256 字节）的对齐存储区
struct alignas(16) AesCtxStorage
{
    unsigned char bytes[kCtx256];
};
} // namespace

Aes::Aes(const std::vector<std::uint8_t>& key) : key_(key)
{
    if (key.size() == 16) key_bits_ = 128;
    else if (key.size() == 24) key_bits_ = 192;
    else if (key.size() == 32) key_bits_ = 256;
}

Aes::Aes(const std::string& key)
    : Aes(std::vector<std::uint8_t>(key.begin(), key.end())) {}

void Aes::cryptEcb(std::uint8_t* buf, std::size_t len, bool enc) const
{
    AesCtxStorage st{};
    auto* p = reinterpret_cast<unsigned char*>(&st);
    if (key_bits_ == 128)
    {
        auto* c = reinterpret_cast<lsx_aes128::AES_ctx*>(p);
        lsx_aes128::AES_init_ctx(c, key_.data());
        for (std::size_t i = 0; i < len; i += kBlock)
            enc ? lsx_aes128::AES_ECB_encrypt(c, buf + i)
                : lsx_aes128::AES_ECB_decrypt(c, buf + i);
    }
    else if (key_bits_ == 192)
    {
        auto* c = reinterpret_cast<lsx_aes192::AES_ctx*>(p);
        lsx_aes192::AES_init_ctx(c, key_.data());
        for (std::size_t i = 0; i < len; i += kBlock)
            enc ? lsx_aes192::AES_ECB_encrypt(c, buf + i)
                : lsx_aes192::AES_ECB_decrypt(c, buf + i);
    }
    else if (key_bits_ == 256)
    {
        auto* c = reinterpret_cast<lsx_aes256::AES_ctx*>(p);
        lsx_aes256::AES_init_ctx(c, key_.data());
        for (std::size_t i = 0; i < len; i += kBlock)
            enc ? lsx_aes256::AES_ECB_encrypt(c, buf + i)
                : lsx_aes256::AES_ECB_decrypt(c, buf + i);
    }
}

void Aes::cryptCbc(std::uint8_t* buf, std::size_t len,
                   const std::uint8_t* iv, bool enc) const
{
    AesCtxStorage st{};
    auto* p = reinterpret_cast<unsigned char*>(&st);
    if (key_bits_ == 128)
    {
        auto* c = reinterpret_cast<lsx_aes128::AES_ctx*>(p);
        lsx_aes128::AES_init_ctx_iv(c, key_.data(), iv);
        enc ? lsx_aes128::AES_CBC_encrypt_buffer(c, buf, len)
            : lsx_aes128::AES_CBC_decrypt_buffer(c, buf, len);
    }
    else if (key_bits_ == 192)
    {
        auto* c = reinterpret_cast<lsx_aes192::AES_ctx*>(p);
        lsx_aes192::AES_init_ctx_iv(c, key_.data(), iv);
        enc ? lsx_aes192::AES_CBC_encrypt_buffer(c, buf, len)
            : lsx_aes192::AES_CBC_decrypt_buffer(c, buf, len);
    }
    else if (key_bits_ == 256)
    {
        auto* c = reinterpret_cast<lsx_aes256::AES_ctx*>(p);
        lsx_aes256::AES_init_ctx_iv(c, key_.data(), iv);
        enc ? lsx_aes256::AES_CBC_encrypt_buffer(c, buf, len)
            : lsx_aes256::AES_CBC_decrypt_buffer(c, buf, len);
    }
}

void Aes::cryptCtr(std::uint8_t* buf, std::size_t len, const std::uint8_t* iv) const
{
    AesCtxStorage st{};
    auto* p = reinterpret_cast<unsigned char*>(&st);
    if (key_bits_ == 128)
    {
        auto* c = reinterpret_cast<lsx_aes128::AES_ctx*>(p);
        lsx_aes128::AES_init_ctx_iv(c, key_.data(), iv);
        lsx_aes128::AES_CTR_xcrypt_buffer(c, buf, len);
    }
    else if (key_bits_ == 192)
    {
        auto* c = reinterpret_cast<lsx_aes192::AES_ctx*>(p);
        lsx_aes192::AES_init_ctx_iv(c, key_.data(), iv);
        lsx_aes192::AES_CTR_xcrypt_buffer(c, buf, len);
    }
    else if (key_bits_ == 256)
    {
        auto* c = reinterpret_cast<lsx_aes256::AES_ctx*>(p);
        lsx_aes256::AES_init_ctx_iv(c, key_.data(), iv);
        lsx_aes256::AES_CTR_xcrypt_buffer(c, buf, len);
    }
}

std::vector<std::uint8_t> Aes::ecbEncrypt(const std::vector<std::uint8_t>& data) const
{
    if (!valid() || data.empty() || data.size() % kBlock != 0) return {};
    auto out = data;
    cryptEcb(out.data(), out.size(), true);
    return out;
}

std::vector<std::uint8_t> Aes::ecbDecrypt(const std::vector<std::uint8_t>& data) const
{
    if (!valid() || data.empty() || data.size() % kBlock != 0) return {};
    auto out = data;
    cryptEcb(out.data(), out.size(), false);
    return out;
}

std::vector<std::uint8_t> pkcs7Pad(const std::vector<std::uint8_t>& data, std::size_t block)
{
    std::size_t pad = block - (data.size() % block);
    auto out = data;
    out.insert(out.end(), pad, static_cast<std::uint8_t>(pad));
    return out;
}

bool pkcs7Unpad(std::vector<std::uint8_t>& data, std::size_t block)
{
    if (data.empty() || data.size() % block != 0) return false;
    std::uint8_t pad = data.back();
    if (pad == 0 || pad > block || pad > data.size()) return false;
    for (std::size_t i = data.size() - pad; i < data.size(); ++i)
        if (data[i] != pad) return false;
    data.resize(data.size() - pad);
    return true;
}

std::vector<std::uint8_t> Aes::cbcEncrypt(const std::vector<std::uint8_t>& data,
                                          const std::vector<std::uint8_t>& iv) const
{
    if (!valid() || iv.size() != kBlock) return {};
    auto out = pkcs7Pad(data);
    cryptCbc(out.data(), out.size(), iv.data(), true);
    return out;
}

std::vector<std::uint8_t> Aes::cbcDecrypt(const std::vector<std::uint8_t>& data,
                                          const std::vector<std::uint8_t>& iv) const
{
    if (!valid() || iv.size() != kBlock || data.empty() || data.size() % kBlock != 0) return {};
    auto out = data;
    cryptCbc(out.data(), out.size(), iv.data(), false);
    if (!pkcs7Unpad(out)) return {};
    return out;
}

std::vector<std::uint8_t> Aes::ctrXcrypt(const std::vector<std::uint8_t>& data,
                                         const std::vector<std::uint8_t>& iv) const
{
    if (!valid() || iv.size() != kBlock) return {};
    auto out = data;
    cryptCtr(out.data(), out.size(), iv.data());
    return out;
}

std::vector<std::uint8_t> Aes::encrypt(const std::vector<std::uint8_t>& data, AesMode mode,
                                       const std::vector<std::uint8_t>& iv) const
{
    switch (mode)
    {
    case AesMode::ECB: return ecbEncrypt(data);
    case AesMode::CBC: return cbcEncrypt(data, iv);
    case AesMode::CTR: return ctrXcrypt(data, iv);
    }
    return {};
}

std::vector<std::uint8_t> Aes::decrypt(const std::vector<std::uint8_t>& data, AesMode mode,
                                       const std::vector<std::uint8_t>& iv) const
{
    switch (mode)
    {
    case AesMode::ECB: return ecbDecrypt(data);
    case AesMode::CBC: return cbcDecrypt(data, iv);
    case AesMode::CTR: return ctrXcrypt(data, iv);   // CTR 加解密同函数
    }
    return {};
}

} // namespace Crypto
} // namespace LSX_LIB
