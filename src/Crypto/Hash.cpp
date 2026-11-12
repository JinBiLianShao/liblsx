/**
 * @file Hash.cpp
 * @brief SHA-256 / HMAC-SHA256 / Base64 / Hex 实现（FIPS 180-4、RFC 2104、RFC 4648）
 */
#include "Crypto/Hash.h"

#include <cstring>

namespace LSX_LIB {
namespace Crypto {

namespace
{
constexpr std::uint32_t K[64] = {
    0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
    0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
    0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
    0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
    0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
    0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
    0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
    0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u};

inline std::uint32_t rotr(std::uint32_t x, unsigned n)
{
    return (x >> n) | (x << (32 - n));
}

void sha256Transform(std::uint32_t state[8], const std::uint8_t block[64])
{
    std::uint32_t w[64];
    for (int i = 0; i < 16; ++i)
        w[i] = (static_cast<std::uint32_t>(block[i*4]) << 24) |
               (static_cast<std::uint32_t>(block[i*4+1]) << 16) |
               (static_cast<std::uint32_t>(block[i*4+2]) << 8) |
               static_cast<std::uint32_t>(block[i*4+3]);
    for (int i = 16; i < 64; ++i)
    {
        std::uint32_t s0 = rotr(w[i-15],7) ^ rotr(w[i-15],18) ^ (w[i-15] >> 3);
        std::uint32_t s1 = rotr(w[i-2],17) ^ rotr(w[i-2],19) ^ (w[i-2] >> 10);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }
    std::uint32_t a=state[0],b=state[1],c=state[2],d=state[3];
    std::uint32_t e=state[4],f=state[5],g=state[6],h=state[7];
    for (int i = 0; i < 64; ++i)
    {
        std::uint32_t S1 = rotr(e,6) ^ rotr(e,11) ^ rotr(e,25);
        std::uint32_t ch = (e & f) ^ (~e & g);
        std::uint32_t t1 = h + S1 + ch + K[i] + w[i];
        std::uint32_t S0 = rotr(a,2) ^ rotr(a,13) ^ rotr(a,22);
        std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        std::uint32_t t2 = S0 + maj;
        h=g; g=f; f=e; e=d+t1;
        d=c; c=b; b=a; a=t1+t2;
    }
    state[0]+=a; state[1]+=b; state[2]+=c; state[3]+=d;
    state[4]+=e; state[5]+=f; state[6]+=g; state[7]+=h;
}

class Sha256Ctx
{
public:
    Sha256Ctx() : state_{0x6a09e667u,0xbb67ae85u,0x3c6ef372u,0xa54ff53au,
                        0x510e527fu,0x9b05688cu,0x1f83d9abu,0x5be0cd19u} {}
    void update(const std::uint8_t* d, std::size_t len)
    {
        total_ += len;
        while (len > 0)
        {
            std::size_t take = std::min(len, static_cast<std::size_t>(64 - buf_len_));
            std::memcpy(buf_ + buf_len_, d, take);
            buf_len_ += take; d += take; len -= take;
            if (buf_len_ == 64)
            {
                sha256Transform(state_, buf_);
                buf_len_ = 0;
            }
        }
    }
    std::array<std::uint8_t, 32> final()
    {
        std::uint64_t bits = total_ * 8;
        std::uint8_t pad = 0x80;
        update(&pad, 1);
        std::uint8_t zero = 0;
        while (buf_len_ != 56) update(&zero, 1);
        std::uint8_t len_be[8];
        for (int i = 0; i < 8; ++i)
            len_be[i] = static_cast<std::uint8_t>(bits >> (56 - i*8));
        update(len_be, 8);
        std::array<std::uint8_t, 32> out{};
        for (int i = 0; i < 8; ++i)
        {
            out[i*4]   = static_cast<std::uint8_t>(state_[i] >> 24);
            out[i*4+1] = static_cast<std::uint8_t>(state_[i] >> 16);
            out[i*4+2] = static_cast<std::uint8_t>(state_[i] >> 8);
            out[i*4+3] = static_cast<std::uint8_t>(state_[i]);
        }
        return out;
    }
private:
    std::uint32_t state_[8];
    std::uint8_t buf_[64]{};
    std::size_t buf_len_ = 0;
    std::uint64_t total_ = 0;
};

const char kHex[] = "0123456789abcdef";
const char kB64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

inline int hexVal(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
} // namespace

std::array<std::uint8_t, 32> sha256(const std::uint8_t* data, std::size_t len)
{
    Sha256Ctx ctx;
    ctx.update(data, len);
    return ctx.final();
}

std::string sha256Hex(const std::uint8_t* data, std::size_t len)
{
    auto d = sha256(data, len);
    return hexEncode(d.data(), d.size());
}

std::array<std::uint8_t, 32> hmacSha256(const std::uint8_t* key, std::size_t key_len,
                                        const std::uint8_t* data, std::size_t data_len)
{
    constexpr std::size_t B = 64;
    std::uint8_t k[B]{};
    if (key_len > B)
    {
        auto h = sha256(key, key_len);
        std::memcpy(k, h.data(), h.size());
    }
    else
    {
        std::memcpy(k, key, key_len);
    }
    std::uint8_t ipad[B], opad[B];
    for (std::size_t i = 0; i < B; ++i)
    {
        ipad[i] = static_cast<std::uint8_t>(k[i] ^ 0x36);
        opad[i] = static_cast<std::uint8_t>(k[i] ^ 0x5C);
    }
    Sha256Ctx inner;
    inner.update(ipad, B);
    inner.update(data, data_len);
    auto ih = inner.final();

    Sha256Ctx outer;
    outer.update(opad, B);
    outer.update(ih.data(), ih.size());
    return outer.final();
}

std::string hmacSha256Hex(const std::string& key, const std::string& data)
{
    auto h = hmacSha256(key, data);
    return hexEncode(h.data(), h.size());
}

std::string base64Encode(const std::uint8_t* data, std::size_t len)
{
    std::string out;
    out.reserve(((len + 2) / 3) * 4);
    std::size_t i = 0;
    while (i + 3 <= len)
    {
        std::uint32_t v = (static_cast<std::uint32_t>(data[i]) << 16) |
                          (static_cast<std::uint32_t>(data[i+1]) << 8) | data[i+2];
        out.push_back(kB64[(v >> 18) & 63]);
        out.push_back(kB64[(v >> 12) & 63]);
        out.push_back(kB64[(v >> 6) & 63]);
        out.push_back(kB64[v & 63]);
        i += 3;
    }
    std::size_t rem = len - i;
    if (rem == 1)
    {
        std::uint32_t v = static_cast<std::uint32_t>(data[i]) << 16;
        out.push_back(kB64[(v >> 18) & 63]);
        out.push_back(kB64[(v >> 12) & 63]);
        out.push_back('=');
        out.push_back('=');
    }
    else if (rem == 2)
    {
        std::uint32_t v = (static_cast<std::uint32_t>(data[i]) << 16) |
                          (static_cast<std::uint32_t>(data[i+1]) << 8);
        out.push_back(kB64[(v >> 18) & 63]);
        out.push_back(kB64[(v >> 12) & 63]);
        out.push_back(kB64[(v >> 6) & 63]);
        out.push_back('=');
    }
    return out;
}

std::vector<std::uint8_t> base64Decode(const std::string& text)
{
    // v0.7.1: 先剥离空白（换行/回车/空格/制表符）
    std::string compact;
    compact.reserve(text.size());
    for (char c : text)
    {
        if (c == 10 || c == 13 || c == 32 || c == 9) continue;
        compact.push_back(c);
    }
    const std::string& text2 = compact;
    std::vector<std::uint8_t> out;
    out.reserve((text2.size() / 4) * 3);
    auto val = [](char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+') return 62;
        if (c == '/') return 63;
        return -1;
    };
    for (std::size_t i = 0; i < text2.size(); i += 4)
    {
        int a = val(text2[i]), b = val(text2[i+1]);
        int c = (text2[i+2] == '=') ? -2 : val(text2[i+2]);
        int d = (text2[i+3] == '=') ? -2 : val(text2[i+3]);
        if (a < 0 || b < 0 || c == -1 || d == -1) return {};
        if (c == -2 && d != -2) return {};   // 非法 padding 顺序
        std::uint32_t v = (static_cast<std::uint32_t>(a) << 18) |
                          (static_cast<std::uint32_t>(b) << 12) |
                          (static_cast<std::uint32_t>(c < 0 ? 0 : c) << 6) |
                          static_cast<std::uint32_t>(d < 0 ? 0 : d);
        out.push_back(static_cast<std::uint8_t>(v >> 16));
        if (c >= 0) out.push_back(static_cast<std::uint8_t>(v >> 8));
        if (d >= 0) out.push_back(static_cast<std::uint8_t>(v));
    }
    return out;
}

std::string hexEncode(const std::uint8_t* data, std::size_t len)
{
    std::string out;
    out.reserve(len * 2);
    for (std::size_t i = 0; i < len; ++i)
    {
        out.push_back(kHex[data[i] >> 4]);
        out.push_back(kHex[data[i] & 0x0F]);
    }
    return out;
}

std::vector<std::uint8_t> hexDecode(const std::string& text)
{
    if (text.size() % 2 != 0) return {};
    std::vector<std::uint8_t> out;
    out.reserve(text.size() / 2);
    for (std::size_t i = 0; i < text.size(); i += 2)
    {
        int h = hexVal(text[i]), l = hexVal(text[i+1]);
        if (h < 0 || l < 0) return {};
        out.push_back(static_cast<std::uint8_t>((h << 4) | l));
    }
    return out;
}

bool constantTimeEquals(const std::uint8_t* a, std::size_t an,
                        const std::uint8_t* b, std::size_t bn)
{
    if (an != bn) return false;
    volatile std::uint8_t diff = 0;
    for (std::size_t i = 0; i < an; ++i) diff |= static_cast<std::uint8_t>(a[i] ^ b[i]);
    return diff == 0;
}

} // namespace Crypto
} // namespace LSX_LIB
