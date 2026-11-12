/**
 * @file Compress.cpp
 * @brief 压缩模块实现（miniz + 自实现 RLE）
 */
#include "Compress/Compress.h"

#include <algorithm>
#include <limits>

#include "miniz.h"   // vendor miniz 3.1.2

namespace LSX_LIB {
namespace Compress {

namespace
{
constexpr std::size_t kMaxInflate = 64u * 1024u * 1024u;

int toMZLevel(Level l)
{
    return std::clamp(static_cast<int>(l), 0, 10);
}
} // namespace

std::vector<std::uint8_t> deflate(const std::uint8_t* data, std::size_t len, Level level)
{
    if (len == 0) return {};
    mz_ulong bound = mz_compressBound(static_cast<mz_ulong>(len));
    std::vector<std::uint8_t> out(static_cast<std::size_t>(bound));
    mz_ulong out_len = static_cast<mz_ulong>(out.size());
    int rc = mz_compress2(out.data(), &out_len, data, static_cast<mz_ulong>(len),
                          toMZLevel(level));
    // mz_compress2 输出 zlib 流（RFC1950）；这里要的是原始 DEFLATE（RFC1951），
    // 去掉 2 字节头与 4 字节 Adler-32 尾
    if (rc != MZ_OK || out_len < 6) return {};
    out.erase(out.begin(), out.begin() + 2);
    out.resize(static_cast<std::size_t>(out_len) - 6);
    return out;
}

std::vector<std::uint8_t> inflate(const std::uint8_t* data, std::size_t len,
                                  std::size_t expected_size)
{
    if (len == 0) return {};
    std::size_t cap = expected_size > 0 ? expected_size : std::max<std::size_t>(len * 4, 1024);
    // expected_size 超过上限时不静默截断成"半成品"：直接让后续扩容逻辑
    // 在达到 kMaxInflate 时明确返回失败，避免误把截断数据当完整解压结果。
    if (expected_size > kMaxInflate) return {};
    cap = std::min(cap, kMaxInflate);
    std::vector<std::uint8_t> out(cap);
    mz_stream s{};
    s.next_in = data;
    s.avail_in = static_cast<unsigned int>(len);
    s.next_out = out.data();
    s.avail_out = static_cast<unsigned int>(out.size());
    if (mz_inflateInit2(&s, -MZ_DEFAULT_WINDOW_BITS) != MZ_OK) return {};  // 原始 DEFLATE

    // miniz 的 MZ_FINISH 在输出缓冲扩容后无法续解（返回 MZ_STREAM_ERROR），
    // 用 MZ_NO_FLUSH 循环驱动：输出满则扩容，输入耗尽即结束。
    while (true)
    {
        int rc = mz_inflate(&s, MZ_NO_FLUSH);
        if (rc != MZ_OK && rc != MZ_STREAM_END && rc != MZ_BUF_ERROR)
        {
            mz_inflateEnd(&s);
            return {};
        }
        if (s.avail_out == 0)
        {
            if (out.size() >= kMaxInflate)
            {
                mz_inflateEnd(&s);
                return {};
            }
            std::size_t old = out.size();
            out.resize(std::min(old * 2, kMaxInflate));
            s.next_out = out.data() + old;
            s.avail_out = static_cast<unsigned int>(out.size() - old);
        }
        if (rc == MZ_STREAM_END) break;
        // 原始 DEFLATE 无尾部长度信息：输入耗尽且已无输出进展时结束
        if (s.avail_in == 0 && s.avail_out > 0 && rc == MZ_BUF_ERROR) break;
    }
    mz_inflateEnd(&s);
    out.resize(static_cast<std::size_t>(s.total_out));
    return out;
}

std::vector<std::uint8_t> zlibCompress(const std::vector<std::uint8_t>& data, Level level)
{
    if (data.empty()) return {};
    mz_ulong bound = mz_compressBound(static_cast<mz_ulong>(data.size()));
    std::vector<std::uint8_t> out(static_cast<std::size_t>(bound));
    mz_ulong out_len = static_cast<mz_ulong>(out.size());
    if (mz_compress2(out.data(), &out_len, data.data(),
                     static_cast<mz_ulong>(data.size()), toMZLevel(level)) != MZ_OK)
        return {};
    out.resize(static_cast<std::size_t>(out_len));
    return out;
}

std::vector<std::uint8_t> zlibDecompress(const std::vector<std::uint8_t>& data)
{
    if (data.empty()) return {};
    std::size_t cap = std::min(std::max<std::size_t>(data.size() * 4, 4096), kMaxInflate);
    std::vector<std::uint8_t> out(cap);
    mz_stream s{};
    s.next_in = data.data();
    s.avail_in = static_cast<unsigned int>(data.size());
    s.next_out = out.data();
    s.avail_out = static_cast<unsigned int>(out.size());
    if (mz_inflateInit(&s) != MZ_OK) return {};   // zlib 封装（RFC1950）
    while (true)
    {
        int rc = mz_inflate(&s, MZ_NO_FLUSH);
        if (rc != MZ_OK && rc != MZ_STREAM_END && rc != MZ_BUF_ERROR)
        {
            mz_inflateEnd(&s);
            return {};
        }
        if (s.avail_out == 0)
        {
            if (out.size() >= kMaxInflate)
            {
                mz_inflateEnd(&s);
                return {};
            }
            std::size_t old = out.size();
            out.resize(std::min(old * 2, kMaxInflate));
            s.next_out = out.data() + old;
            s.avail_out = static_cast<unsigned int>(out.size() - old);
        }
        if (rc == MZ_STREAM_END) break;   // zlib 尾部校验完成后才结束，不能因 avail_in==0 提前退出
    }
    mz_inflateEnd(&s);
    out.resize(static_cast<std::size_t>(s.total_out));
    return out;
}

std::vector<std::uint8_t> gzipCompress(const std::vector<std::uint8_t>& data, Level level)
{
    // gzip = 10 字节头 + 原始 DEFLATE + CRC32(4) + ISIZE(4)
    auto body = deflate(data.data(), data.size(), level);
    if (body.empty()) return {};
    std::vector<std::uint8_t> out;
    out.reserve(10 + body.size() + 8);
    const std::uint8_t hdr[10] = {0x1F, 0x8B, 8, 0, 0, 0, 0, 0, 0, 3};  // mtime=0, OS=Unix
    out.insert(out.end(), hdr, hdr + 10);
    out.insert(out.end(), body.begin(), body.end());
    std::uint32_t crc = crc32(data.data(), data.size());
    std::uint32_t isize = static_cast<std::uint32_t>(data.size() & 0xFFFFFFFFu);
    for (int i = 0; i < 4; ++i) out.push_back(static_cast<std::uint8_t>(crc >> (i*8)));
    for (int i = 0; i < 4; ++i) out.push_back(static_cast<std::uint8_t>(isize >> (i*8)));
    return out;
}

std::vector<std::uint8_t> gzipDecompress(const std::vector<std::uint8_t>& data)
{
    if (data.size() < 18) return {};
    if (data[0] != 0x1F || data[1] != 0x8B || data[2] != 8) return {};
    const std::uint8_t flg = data[3];
    // 按 RFC 1952 跳过全部可选头字段（FEXTRA / FNAME / FCOMMENT / FHCRC），
    // 否则带文件名的标准 gzip（gzip 命令默认带 FNAME）会解压错位或失败。
    std::size_t hdr = 10;
    if (flg & 0x04)  // FEXTRA: 2 字节长度 + 变长数据
    {
        if (hdr + 2 > data.size()) return {};
        std::size_t xlen = static_cast<std::size_t>(data[hdr]) |
                           (static_cast<std::size_t>(data[hdr + 1]) << 8);
        hdr += 2 + xlen;
    }
    if (flg & 0x08)  // FNAME: 以 0 结尾的字符串
    {
        while (hdr < data.size() && data[hdr] != 0) ++hdr;
        ++hdr;
    }
    if (flg & 0x10)  // FCOMMENT: 以 0 结尾的字符串
    {
        while (hdr < data.size() && data[hdr] != 0) ++hdr;
        ++hdr;
    }
    if (flg & 0x02) hdr += 2;  // FHCRC: 2 字节头 CRC
    if (hdr + 8 > data.size()) return {};  // 至少留出 8 字节尾部
    auto body = inflate(data.data() + hdr, data.size() - hdr - 8);
    if (body.empty()) return {};
    // 尾部 CRC32 与 ISIZE 校验
    std::uint32_t crc = 0, isize = 0;
    const auto* tail = data.data() + data.size() - 8;
    for (int i = 0; i < 4; ++i) crc |= static_cast<std::uint32_t>(tail[i]) << (i*8);
    for (int i = 0; i < 4; ++i) isize |= static_cast<std::uint32_t>(tail[4+i]) << (i*8);
    if (crc32(body.data(), body.size()) != crc) return {};
    if (static_cast<std::uint32_t>(body.size()) != isize) return {};
    return body;
}

std::vector<std::uint8_t> rleEncode(const std::uint8_t* data, std::size_t len)
{
    std::vector<std::uint8_t> out;
    if (len == 0) return out;
    out.reserve(len);
    std::size_t i = 0;
    while (i < len)
    {
        std::uint8_t v = data[i];
        std::size_t run = 1;
        while (i + run < len && data[i + run] == v && run < 255) ++run;
        out.push_back(v);
        out.push_back(static_cast<std::uint8_t>(run));
        i += run;
    }
    return out;
}

std::vector<std::uint8_t> rleDecode(const std::uint8_t* data, std::size_t len)
{
    if (len % 2 != 0) return {};
    std::vector<std::uint8_t> out;
    for (std::size_t i = 0; i < len; i += 2)
    {
        if (data[i+1] == 0) return {};   // count=0 非法
        // 恶意输入（每对 count=255）可致 127.5 倍内存放大且无上限，
        // 用与 inflate 一致的 kMaxInflate 上限约束输出，超出即判失败。
        if (static_cast<std::size_t>(data[i+1]) > kMaxInflate - out.size()) return {};
        out.insert(out.end(), data[i+1], data[i]);
    }
    return out;
}

std::uint32_t crc32(const std::uint8_t* data, std::size_t len)
{
    return static_cast<std::uint32_t>(mz_crc32(MZ_CRC32_INIT, data, len));
}

std::uint32_t adler32(const std::uint8_t* data, std::size_t len)
{
    return static_cast<std::uint32_t>(mz_adler32(MZ_ADLER32_INIT, data, len));
}

} // namespace Compress
} // namespace LSX_LIB
