# LIBLSX 工具库 Crypto 模块使用说明

## 模块概述

Crypto 提供嵌入式设备常用的对称加密、摘要、消息认证与编码能力。AES 基于 vendor 的 **tiny-AES-c**（Unlicense，公有领域），SHA-256/HMAC/Base64/Hex 为库内纯实现。全部随库编译，目标板无需 OpenSSL 等外部动态库。命名空间：LSX_LIB::Crypto。

| 组件 | 能力 | 来源 |
|------|------|------|
| Aes | AES-128/192/256，ECB/CBC/CTR | tiny-AES-c（Unlicense） |
| sha256 / sha256Hex | SHA-256 摘要 | 库内实现（FIPS 180-4） |
| hmacSha256 / hmacSha256Hex | HMAC-SHA256 | 库内实现（RFC 2104） |
| base64Encode / base64Decode | Base64 | 库内实现（RFC 4648） |
| hexEncode / hexDecode | 十六进制 | 库内实现 |
| constantTimeEquals | 常量时间比较 | 库内实现 |

## AES 使用说明

```cpp
#include "Crypto/Aes.h"
using namespace LSX_LIB::Crypto;

std::vector<std::uint8_t> key(16, 0x2B);   // 16/24/32 字节 → AES-128/192/256
std::vector<std::uint8_t> iv(16, 0x00);
Aes aes(key);
if (!aes.valid()) { /* 密钥长度非法 */ }

// CBC（推荐）：自动 PKCS#7 填充，任意长度输入
auto cipher = aes.encrypt(plaintext, AesMode::CBC, iv);
auto plain  = aes.decrypt(cipher,   AesMode::CBC, iv);

// CTR：流加密，无需填充，任意长度
auto stream = aes.encrypt(data, AesMode::CTR, iv);
auto back   = aes.decrypt(stream, AesMode::CTR, iv);  // 同一函数

// ECB：仅用于兼容旧协议（不安全），输入必须 16 字节对齐
auto blk = aes.ecbEncrypt(block16);
```

### 安全须知

- **ECB 不具备语义安全性**，相同明文块产生相同密文块，新设计一律用 CBC/CTR。
- **IV 对同一密钥绝不可复用**；CBC 的 IV 应随机，CTR 的 IV/计数器应唯一。
- 本模块只提供加密原语，**不提供认证加密**（如 GCM）。需要防篡改请配合 HMAC：密文 + HMAC-SHA256(key_mac, 密文)，接收端先验 MAC 再解密。
- CBC 解密带 PKCS#7 填充校验，篡改末块会返回空 vector（但不等于认证，仍应验 MAC）。

### AES API 参考

| 方法 | 说明 |
|------|------|
| Aes(key) / valid() | 构造与有效性 |
| ecbEncrypt / ecbDecrypt | 16 字节对齐块 |
| cbcEncrypt / cbcDecrypt | 自动 PKCS#7 填充 |
| ctrXcrypt | 流加密（加解密同函数） |
| encrypt / decrypt(data, mode, iv) | 统一入口 |
| pkcs7Pad / pkcs7Unpad | 填充工具 |

## 摘要与 HMAC

```cpp
#include "Crypto/Hash.h"

std::string h = sha256Hex("abc");       // ba7816bf...15ad
std::string mac = hmacSha256Hex("key", "what do ya want for nothing?");
```

SHA-256 支持任意长度输入（已用 100 万字节标准向量验证）；HMAC 密钥超 64 字节自动先哈希。constantTimeEquals(a, an, b, bn) 用于比较 MAC/标签，避免时序侧信道。

## Base64 / Hex

```cpp
auto enc = base64Encode(raw_bytes);     // 二进制安全
auto dec = base64Decode(enc);           // 空 = 输入非法
auto hx = hexEncode(bytes);             // 小写
auto raw = hexDecode(hx);               // 大小写输入均可
```

## 与 Serialize / Protocol 组合

典型安全帧：FrameParser 切帧 → HMAC 验签 → AES 解密 → Serialize 解码载荷。

## 第三方许可

tiny-AES-c 为 Unlicense（公有领域），miniz 为 MIT，均见 Include/third_party/README.md。本模块不引入任何需要目标板额外安装的动态库。
