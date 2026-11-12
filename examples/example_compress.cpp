/**
 * @file example_compress.cpp
 * @brief LIBLSX Compress 模块使用示例
 *
 * 演示内容：
 *   deflate/inflate（原始 DEFLATE）、zlib、gzip（带 CRC32 校验）、RLE、crc32/adler32。
 *
 * 关键点：
 *   - 全部返回 vector<uint8_t>，失败返回空 vector（不抛异常）。
 *   - gzip 自带 CRC32 + ISIZE 完整性校验，解压失败（校验不过）返回空。
 *   - RLE 适合重复度高的数据（如 ADC 采样），对不重复数据会膨胀，选型注意。
 */

#include "Compress/Compress.h"

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace cp = LSX_LIB::Compress;

int main()
{
    std::cout << "===== Compress 压缩模块示例 =====\n";

    // 构造一段可压缩的数据（重复度高）
    std::string raw = "LIBLSX is a lightweight C++ utility library. ";
    for (int i = 0; i < 20; ++i) raw += raw.substr(0, 40);   // 制造重复

    // ---- 1) 原始 DEFLATE ----
    auto d = cp::deflate(raw);
    auto d_back = cp::inflate(d);
    std::cout << "  deflate: " << raw.size() << " -> " << d.size() << " 字节"
              << "，解压回 " << d_back.size() << " 字节，一致=" 
              << (std::string(d_back.begin(), d_back.end()) == raw ? "是" : "否") << "\n";

    // ---- 2) zlib ----
    auto z = cp::zlibCompress({raw.begin(), raw.end()});
    auto z_back = cp::zlibDecompress(z);
    std::cout << "  zlib: " << raw.size() << " -> " << z.size() << " 字节"
              << "，解压回 " << z_back.size() << " 字节\n";

    // ---- 3) gzip（带 CRC32 完整性校验）----
    auto g = cp::gzipCompress({raw.begin(), raw.end()});
    auto g_back = cp::gzipDecompress(g);
    std::cout << "  gzip: " << raw.size() << " -> " << g.size() << " 字节"
              << "，解压回 " << g_back.size() << " 字节，一致=" 
              << (std::string(g_back.begin(), g_back.end()) == raw ? "是" : "否") << "\n";

    // ---- 4) RLE（重复数据专用）----
    std::vector<std::uint8_t> runs = {0xAA, 0xAA, 0xAA, 0xAA, 0xBB, 0xBB};
    auto r = cp::rleEncode(runs);
    auto r_back = cp::rleDecode(r);
    std::cout << "  RLE: " << runs.size() << " -> " << r.size() << " 字节"
              << "，解压回 " << r_back.size() << " 字节，一致=" 
              << (r_back == runs ? "是" : "否") << "\n";

    // ---- 5) crc32 / adler32 ----
    std::cout << "  crc32(raw) = 0x" << std::hex
              << cp::crc32(reinterpret_cast<const std::uint8_t*>(raw.data()), raw.size())
              << std::dec << "\n";

    return 0;
}
