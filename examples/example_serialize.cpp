/**
 * @file example_serialize.cpp
 * @brief LIBLSX Serialize 模块使用示例
 *
 * 演示内容：
 *   1. 定宽整数（小端）/ 浮点 / 布尔 编解码
 *   2. varint（LEB128）与 zigzag 有符号编码
 *   3. 长度前缀字符串与字节数组
 *   4. TLV 字段（对齐 protobuf wire format，未知字段可跳过）
 *   5. 读取失败置 fail 标志、不抛异常（解析不可信输入）
 *
 * 关键点：
 *   - 多字节整数一律小端；字符串/字节数组是「varint 长度 + 原始字节」。
 *   - BinaryReader 任何越界/格式错误只置 fail()，后续读返回零值，调用方最后统一检查。
 */

#include "Serialize/Serialize.h"

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace se = LSX_LIB::Serialize;

int main()
{
    std::cout << "===== Serialize 二进制序列化示例 =====\n";

    // ---- 1) 写入 ----
    se::BinaryWriter w;
    w.writeU8(0xAB);                    // 定宽整数（小端）
    w.writeU32(0x12345678);
    w.writeI32(-42);                    // 有符号定宽
    w.writeF32(3.5f);                   // 浮点
    w.writeBool(true);                  // 布尔（1 字节）
    w.writeVarint(300);                 // varint：300 -> 2 字节
    w.writeVarintZZ(-3);                // zigzag：-3 -> 5（省字节）
    w.writeString("hello");             // 长度前缀 + 字节
    w.writeField(1, static_cast<std::uint64_t>(7));  // TLV 字段 tag=1（显式 uint64 消歧义）
    w.writeField(2, std::string("device-01"));       // TLV 字符串字段

    std::cout << "  编码总字节数: " << w.size() << "\n";

    // ---- 2) 对称读取 ----
    se::BinaryReader r(w.data().data(), w.size());
    std::cout << "  readU8 = 0x" << std::hex << static_cast<int>(r.readU8()) << std::dec << "\n";
    std::cout << "  readU32 = 0x" << std::hex << r.readU32() << std::dec << "\n";
    std::cout << "  readI32 = " << r.readI32() << "\n";
    std::cout << "  readF32 = " << r.readF32() << "\n";
    std::cout << "  readBool = " << (r.readBool() ? "true" : "false") << "\n";
    std::cout << "  readVarint = " << r.readVarint() << "\n";
    std::cout << "  readVarintZZ = " << r.readVarintZZ() << "\n";
    std::cout << "  readString = " << r.readString() << "\n";

    auto f1 = r.readField();
    std::cout << "  field tag=" << f1.tag << " varint=" << f1.varint << "\n";
    auto f2 = r.readField();
    std::cout << "  field tag=" << f2.tag
              << " bytes=" << std::string(f2.bytes.begin(), f2.bytes.end()) << "\n";

    std::cout << "  读取后 fail 标志: " << (r.fail() ? "置位" : "未置位") << "\n";

    // ---- 3) 不可信输入：越界不抛异常，只置 fail ----
    const std::uint8_t bad[] = {0xFF, 0xFF};   // 声称 varint 却截断
    se::BinaryReader br(bad, sizeof(bad));
    auto v = br.readVarint();
    std::cout << "  损坏输入 readVarint=" << v << "，fail=" 
              << (br.fail() ? "true（应检查）" : "false") << "\n";

    return 0;
}
