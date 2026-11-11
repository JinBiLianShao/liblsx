/**
 * @file aes_variants.cpp
 * @brief [LIBLSX 本地适配] 把上游 tiny-AES-c 编译成 128/192/256 三个命名空间实例。
 * @details 上游用编译期宏 AES128/AES192/AES256 决定密钥长度，一份源码只能有一种，
 * 且 AES_ctx 的 RoundKey 大小随密钥长度变化。这里每个实例把 aes.h 与 aes.c
 * 一起包进独立 namespace，生成互不冲突的三套符号。
 * 每次展开前 #undef 会重定义的宏，避免 GCC 的宏重定义警告。
 * 上游源码：kokke/tiny-AES-c（Unlicense），见 Include/third_party/README.md。
 */
#include <cstdint>
#include <cstring>

// 每个实例展开前清理：include guard 与所有按密钥长度变化的宏
#define LSX_AES_RESET() \
    _Pragma("GCC diagnostic push") \
    _Pragma("GCC diagnostic ignored "-Wpragmas"")
// 说明：下游宏全部在展开前 #undef，GCC 不会产生重定义警告，无需 diagnostic 包装。
#undef LSX_AES_RESET

#undef _AES_H_
#undef AES_KEYLEN
#undef AES_keyExpSize
#undef AES_BLOCKLEN
#undef Nk
#undef Nr
#undef Nb
#define AES128 1
namespace lsx_aes128 {
#include "aes.h"
#include "aes.c"
}
#undef AES128

#undef _AES_H_
#undef AES_KEYLEN
#undef AES_keyExpSize
#undef AES_BLOCKLEN
#undef Nk
#undef Nr
#undef Nb
#define AES192 1
namespace lsx_aes192 {
#include "aes.h"
#include "aes.c"
}
#undef AES192

#undef _AES_H_
#undef AES_KEYLEN
#undef AES_keyExpSize
#undef AES_BLOCKLEN
#undef Nk
#undef Nr
#undef Nb
#define AES256 1
namespace lsx_aes256 {
#include "aes.h"
#include "aes.c"
}
#undef AES256
