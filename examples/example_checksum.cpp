/**
 * @file example_checksum.cpp
 * @brief LIBLSX Checksum 模块使用示例
 *
 * 演示内容：
 *   CRC8 / CRC16-Modbus / CRC16-CCITT-FALSE / CRC32（增量式），
 *   以及 sum8 / sum8Complement / xor8 / fletcher16 简单校验。
 *
 * 关键点：
 *   - 全部为增量式接口（update 可分段喂数据），适合流式帧校验。
 *   - 类形式（Crc32 等）实例内含状态，多线程各持各的实例；
 *     或直接用静态 calc() 一次性计算。
 *   - 计算表为 constexpr 生成，无运行时初始化开销、无动态分配。
 */

#include "Checksum/Checksum.h"

#include <cstdint>
#include <iostream>

namespace ck = LSX_LIB::Checksum;

int main()
{
    std::cout << "===== Checksum 校验算法示例 =====\n";

    // 待校验数据
    const std::uint8_t data[] = {0x31, 0x32, 0x33, 0x34};   // "1234"
    const std::size_t len = sizeof(data);

    // ---- 1) 一次性计算（静态 calc）----
    std::cout << "  CRC8          = 0x" << std::hex
              << static_cast<int>(ck::Crc8::calc(data, len)) << std::dec << "\n";
    std::cout << "  CRC16-Modbus  = 0x" << std::hex
              << ck::Crc16Modbus::calc(data, len) << std::dec << "\n";
    std::cout << "  CRC16-CCITT   = 0x" << std::hex
              << ck::Crc16CcittFalse::calc(data, len) << std::dec << "\n";
    std::cout << "  CRC32         = 0x" << std::hex
              << ck::Crc32::calc(data, len) << std::dec << "\n";
    std::cout << "  sum8          = 0x" << std::hex
              << static_cast<int>(ck::sum8(data, len)) << std::dec << "\n";
    std::cout << "  xor8          = 0x" << std::hex
              << static_cast<int>(ck::xor8(data, len)) << std::dec << "\n";
    std::cout << "  fletcher16    = 0x" << std::hex
              << ck::fletcher16(data, len) << std::dec << "\n";

    // ---- 2) 增量式：分段喂数据，结果与一次性一致 ----
    ck::Crc32 crc;
    crc.update(data, 2);          // 先喂前 2 字节
    crc.update(data + 2, 2);      // 再喂后 2 字节
    std::cout << "  CRC32 分段喂入 = 0x" << std::hex << crc.value() << std::dec;
    std::cout << "（应与上面一次性结果一致）\n";

    // ---- 3) Modbus 帧字节序：线上低字节在前 ----
    ck::Crc16Modbus mb;
    mb.update(data, len);
    std::cout << "  Modbus 线上低字节在前 = 0x" << std::hex << mb.littleEndian();
    std::cout << std::dec << "\n";

    return 0;
}
