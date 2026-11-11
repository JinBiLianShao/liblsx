/**
 * @file test_crypto.cpp
 * @brief Crypto 模块深度测试：AES 标准向量 / 三密钥长度 / 摘要 / HMAC / 编码
 */
#include "test_framework.h"

#include <string>
#include <vector>

#include "Crypto/Aes.h"
#include "Crypto/Hash.h"

using namespace LSX_LIB::Crypto;

static std::vector<std::uint8_t> V(std::initializer_list<std::uint8_t> l)
{
    return std::vector<std::uint8_t>(l);
}

// ---------- AES ----------

LSX_TEST(Aes, InvalidKeyLengthRejected) {
    Aes bad(std::vector<std::uint8_t>(15, 1));
    LSX_EXPECT_TRUE(!bad.valid());
    LSX_EXPECT_TRUE(bad.ecbEncrypt(V({0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15})).empty());
}

LSX_TEST(Aes, EcbStandardVectorAes128) {
    // FIPS 197 附录 C.1：key 000102...0f, plaintext 00112233445566778899aabbccddeeff
    // 密文 69c4e0d86a7b0430d8cdb78070b4c55a
    std::vector<std::uint8_t> key(16), plain(16), expect(16);
    for (int i = 0; i < 16; ++i) { key[i] = i; plain[i] = i * 0x11; }
    std::uint8_t exp[16] = {0x69,0xc4,0xe0,0xd8,0x6a,0x7b,0x04,0x30,
                            0xd8,0xcd,0xb7,0x80,0x70,0xb4,0xc5,0x5a};
    Aes aes(key);
    auto cipher = aes.ecbEncrypt(plain);
    LSX_EXPECT_EQ(cipher.size(), 16u);
    for (int i = 0; i < 16; ++i) LSX_EXPECT_EQ(cipher[i], exp[i]);
    auto back = aes.ecbDecrypt(cipher);
    for (int i = 0; i < 16; ++i) LSX_EXPECT_EQ(back[i], plain[i]);
}

LSX_TEST(Aes, EcbStandardVectorAes192) {
    // FIPS 197 附录 C.2：密文 dda97ca4864cdfe06eaf70a0ec0d7191
    std::vector<std::uint8_t> key(24), plain(16);
    for (int i = 0; i < 24; ++i) key[i] = i;
    for (int i = 0; i < 16; ++i) plain[i] = i * 0x11;
    std::uint8_t exp[16] = {0xdd,0xa9,0x7c,0xa4,0x86,0x4c,0xdf,0xe0,
                            0x6e,0xaf,0x70,0xa0,0xec,0x0d,0x71,0x91};
    Aes aes(key);
    auto cipher = aes.ecbEncrypt(plain);
    LSX_EXPECT_EQ(cipher.size(), 16u);
    for (int i = 0; i < 16; ++i) LSX_EXPECT_EQ(cipher[i], exp[i]);
}

LSX_TEST(Aes, EcbStandardVectorAes256) {
    // FIPS 197 附录 C.3：密文 8ea2b7ca516745bfeafc49904b496089
    std::vector<std::uint8_t> key(32), plain(16);
    for (int i = 0; i < 32; ++i) key[i] = i;
    for (int i = 0; i < 16; ++i) plain[i] = i * 0x11;
    std::uint8_t exp[16] = {0x8e,0xa2,0xb7,0xca,0x51,0x67,0x45,0xbf,
                            0xea,0xfc,0x49,0x90,0x4b,0x49,0x60,0x89};
    Aes aes(key);
    auto cipher = aes.ecbEncrypt(plain);
    LSX_EXPECT_EQ(cipher.size(), 16u);
    for (int i = 0; i < 16; ++i) LSX_EXPECT_EQ(cipher[i], exp[i]);
    auto back = aes.ecbDecrypt(cipher);
    for (int i = 0; i < 16; ++i) LSX_EXPECT_EQ(back[i], plain[i]);
}

LSX_TEST(Aes, CbcRoundtripAllKeySizes) {
    for (std::size_t klen : {16u, 24u, 32u})
    {
        std::vector<std::uint8_t> key(klen), iv(16), plain;
        for (std::size_t i = 0; i < klen; ++i) key[i] = static_cast<std::uint8_t>(i * 3 + 1);
        for (int i = 0; i < 16; ++i) iv[i] = static_cast<std::uint8_t>(0xF0 - i);
        plain = std::vector<std::uint8_t>(37);   // 非整块，触发 PKCS#7
        for (std::size_t i = 0; i < plain.size(); ++i) plain[i] = static_cast<std::uint8_t>(i);
        Aes aes(key);
        auto cipher = aes.encrypt(plain, AesMode::CBC, iv);
        LSX_EXPECT_TRUE(!cipher.empty());
        LSX_EXPECT_EQ(cipher.size() % 16u, 0u);
        auto back = aes.decrypt(cipher, AesMode::CBC, iv);
        LSX_EXPECT_TRUE(back == plain);
    }
}

LSX_TEST(Aes, CbcStandardVectorNist80038a) {
    // NIST SP 800-38A F.2.1 CBC-AES128 首块
    std::uint8_t key_b[16] = {0x2b,0x7e,0x15,0x16,0x28,0xae,0xd2,0xa6,
                              0xab,0xf7,0x15,0x88,0x09,0xcf,0x4f,0x3c};
    std::uint8_t iv_b[16] = {0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
                             0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f};
    std::uint8_t pt[16] = {0x6b,0xc1,0xbe,0xe2,0x2e,0x40,0x9f,0x96,
                           0xe9,0x3d,0x7e,0x11,0x73,0x93,0x17,0x2a};
    std::uint8_t exp[16] = {0x76,0x49,0xab,0xac,0x81,0x19,0xb2,0x46,
                            0xce,0xe9,0x8e,0x9b,0x12,0xe9,0x19,0x7d};
    Aes aes(std::vector<std::uint8_t>(key_b, key_b + 16));
    std::vector<std::uint8_t> iv(iv_b, iv_b + 16);
    // 直接对单块做 CBC（无填充）需走 IV 初始化路径：用 ecb 无法表达，
    // 这里验证 cbcEncrypt 的填充结果首块与标准向量一致
    auto out = aes.cbcEncrypt(std::vector<std::uint8_t>(pt, pt + 16), iv);
    LSX_EXPECT_TRUE(out.size() >= 16u);
    for (int i = 0; i < 16; ++i) LSX_EXPECT_EQ(out[i], exp[i]);
}

LSX_TEST(Aes, CtrRoundtrip) {
    std::vector<std::uint8_t> key(16, 0x42), iv(16, 0x01);
    std::vector<std::uint8_t> plain(53);   // 任意长度，CTR 无需填充
    for (std::size_t i = 0; i < plain.size(); ++i) plain[i] = static_cast<std::uint8_t>(i);
    Aes aes(key);
    auto c = aes.ctrXcrypt(plain, iv);
    LSX_EXPECT_EQ(c.size(), plain.size());
    LSX_EXPECT_TRUE(c != plain);
    auto back = aes.ctrXcrypt(c, iv);      // CTR 加解密同一函数
    LSX_EXPECT_TRUE(back == plain);
}

LSX_TEST(Aes, BadIvRejected) {
    Aes aes(std::vector<std::uint8_t>(16, 1));
    LSX_EXPECT_TRUE(aes.cbcEncrypt(V({1,2,3}), V({1,2,3})).empty());  // IV 非 16 字节
    LSX_EXPECT_TRUE(aes.ctrXcrypt(V({1,2,3}), V({1,2,3})).empty());
}

LSX_TEST(Aes, Pkcs7PadUnpad) {
    auto padded = pkcs7Pad(V({1,2,3,4,5}));
    LSX_EXPECT_EQ(padded.size(), 16u);
    LSX_EXPECT_EQ(padded.back(), std::uint8_t(11));
    LSX_EXPECT_TRUE(pkcs7Unpad(padded));
    LSX_EXPECT_EQ(padded.size(), 5u);
    // 坏填充
    auto bad = V({1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,9});
    LSX_EXPECT_TRUE(!pkcs7Unpad(bad));
}

LSX_TEST(Aes, TamperedCbcDetectedByPadding) {
    std::vector<std::uint8_t> key(16, 7), iv(16, 3);
    std::vector<std::uint8_t> plain(30, 0xAB);
    Aes aes(key);
    auto c = aes.cbcEncrypt(plain, iv);
    c[c.size() - 1] ^= 0xFF;               // 篡改最后一个密文块
    auto back = aes.cbcDecrypt(c, iv);
    LSX_EXPECT_TRUE(back.empty());          // 填充校验失败 → 返回空
}

// ---------- SHA-256 ----------

LSX_TEST(Hash, Sha256StandardVectors) {
    LSX_EXPECT_EQ(sha256Hex(""), std::string("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
    LSX_EXPECT_EQ(sha256Hex("abc"), std::string("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
}

LSX_TEST(Hash, Sha256LongInput) {
    // 100 万个 'a' → 标准值 cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0
    std::string s(1000000, 'a');
    LSX_EXPECT_EQ(sha256Hex(s), std::string("cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0"));
}

LSX_TEST(Hash, HmacRfc4231Vector) {
    // RFC 4231 test case 2: key="Jefe", data="what do ya want for nothing?"
    LSX_EXPECT_EQ(hmacSha256Hex("Jefe", "what do ya want for nothing?"),
                  std::string("5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843"));
}

LSX_TEST(Hash, HmacKeyLongerThanBlock) {
    // 超 64 字节密钥走 hash 分支，与短密钥结果不同但可稳定复现
    std::string longkey(100, 'k');
    auto h1 = hmacSha256(longkey, "data");
    auto h2 = hmacSha256(longkey, "data");
    LSX_EXPECT_TRUE(h1 == h2);
    LSX_EXPECT_TRUE(h1 != hmacSha256("k", "data"));
}

// ---------- Base64 / Hex ----------

LSX_TEST(Hash, Base64StandardVectors) {
    LSX_EXPECT_EQ(base64Encode(""), std::string(""));
    LSX_EXPECT_EQ(base64Encode("f"), std::string("Zg=="));
    LSX_EXPECT_EQ(base64Encode("fo"), std::string("Zm8="));
    LSX_EXPECT_EQ(base64Encode("foo"), std::string("Zm9v"));
    LSX_EXPECT_EQ(base64Encode("hello"), std::string("aGVsbG8="));
}

LSX_TEST(Hash, Base64RoundtripBinary) {
    std::vector<std::uint8_t> raw(256);
    for (int i = 0; i < 256; ++i) raw[i] = static_cast<std::uint8_t>(i);
    auto enc = base64Encode(raw.data(), raw.size());
    auto dec = base64Decode(enc);
    LSX_EXPECT_TRUE(dec == raw);
}

LSX_TEST(Hash, Base64RejectsInvalid) {
    LSX_EXPECT_TRUE(base64Decode("abc").empty());      // 长度非 4 倍数
    LSX_EXPECT_TRUE(base64Decode("Zm9v!").empty());    // 非法字符
    LSX_EXPECT_TRUE(base64Decode("Z===").empty());     // 非法 padding
}

LSX_TEST(Hash, HexRoundtrip) {
    std::vector<std::uint8_t> raw = {0x00, 0x0F, 0xAB, 0xFF, 0x10};
    auto enc = hexEncode(raw);
    LSX_EXPECT_EQ(enc, std::string("000fabff10"));
    auto dec = hexDecode(enc);
    LSX_EXPECT_TRUE(dec == raw);
    LSX_EXPECT_TRUE(hexDecode("0A0bFF").size() == 3u);   // 大小写混合
    LSX_EXPECT_TRUE(hexDecode("abc").empty());             // 奇数长度
    LSX_EXPECT_TRUE(hexDecode("zz").empty());              // 非法字符
}

LSX_TEST(Hash, ConstantTimeEquals) {
    LSX_EXPECT_TRUE(constantTimeEquals(reinterpret_cast<const std::uint8_t*>("abc"), 3,
                                      reinterpret_cast<const std::uint8_t*>("abc"), 3));
    LSX_EXPECT_TRUE(!constantTimeEquals(reinterpret_cast<const std::uint8_t*>("abc"), 3,
                                       reinterpret_cast<const std::uint8_t*>("abd"), 3));
    LSX_EXPECT_TRUE(!constantTimeEquals(reinterpret_cast<const std::uint8_t*>("abc"), 3,
                                       reinterpret_cast<const std::uint8_t*>("ab"), 2));
}
