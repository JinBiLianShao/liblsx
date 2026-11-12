/**
 * @file example_protocol.cpp
 * @brief LIBLSX Protocol 模块使用示例
 *
 * 演示内容：
 *   1. 帧头 + 帧尾 的流式帧解析（逐字节 feed，天然处理粘包/半包）
 *   2. 长度字段定帧（len_offset + len_bytes）
 *   3. 校验回调（CRC 校验失败自动丢弃帧）
 *   4. 一次性批量解析工具 extractFrames
 *
 * 关键点：
 *   - feed 逐字节喂入，状态机内部切帧；超长帧/校验失败自动丢弃并计数。
 *   - 无动态分配，可与 Checksum、Serialize 直接组合。
 */

#include "Protocol/FrameParser.h"
#include "Checksum/Checksum.h"

#include <cstdint>
#include <iostream>
#include <vector>

namespace pr = LSX_LIB::Protocol;

int main()
{
    std::cout << "===== Protocol 帧解析示例 =====\n";

    // ---- 1) 帧头 AA 55 + 帧尾 0D 0A ----
    pr::FrameParser parser(64);          // 单帧最大 64 字节
    parser.setHeader({0xAA, 0x55});
    parser.setEnder({0x0D, 0x0A});

    // 帧回调：每切出一帧调用一次
    parser.onFrame([](const std::uint8_t* frame, std::size_t len) {
        std::cout << "  收到一帧 " << len << " 字节: ";
        for (std::size_t i = 0; i < len; ++i) {
            std::cout << std::hex << static_cast<int>(frame[i]) << " ";
        }
        std::cout << std::dec << "\n";
    });

    // 模拟网络流：两帧粘在一起，还带半帧（逐字节 feed）
    std::vector<std::uint8_t> stream = {
        0xAA, 0x55, 0x01, 0x02, 0x0D, 0x0A,   // 帧1
        0xAA, 0x55, 0x03, 0x04, 0x05, 0x0D, 0x0A,   // 帧2
        0xAA, 0x55, 0x06                      // 帧3 的半截（无帧尾，留在缓冲）
    };
    for (auto b : stream) parser.feed(b);

    std::cout << "  已接受帧: " << parser.framesAccepted()
              << "，拒绝帧: " << parser.framesRejected() << "\n";
    std::cout << "  半帧残留字节: " << parser.pending() << "\n";

    // ---- 2) 长度字段定帧 + 校验回调 ----
    pr::FrameParser p2(64);
    p2.setHeader({0xAA, 0x55});
    p2.setLengthField(0, 1);             // 帧头后 1 字节是长度字段
    p2.setCheckCallback([](const std::uint8_t* content, std::size_t len) {
        // 用 sum8 校验：最后 1 字节应是前面字节的累加和
        if (len < 1) return false;
        auto sum = LSX_LIB::Checksum::sum8(content, len - 1);
        return sum == content[len - 1];
    });
    p2.onFrame([](const std::uint8_t* f, std::size_t n) {
        (void)f;   // 本示例不关心帧内容，只打印长度
        std::cout << "  [len帧] 收到 " << n << " 字节（校验通过）\n";
    });

    // 帧：AA 55 | len=3 | 0x10 0x20 | checksum
    // 校验回调收到的 content = [len=0x03, 0x10, 0x20, checksum]，
    // 前 3 字节累加和 = 0x03+0x10+0x20 = 0x33，故 checksum 应为 0x33
    p2.feed(0xAA); p2.feed(0x55); p2.feed(0x03);
    p2.feed(0x10); p2.feed(0x20); p2.feed(0x33);
    std::cout << "  长度帧已接受: " << p2.framesAccepted() << "\n";

    // ---- 3) 批量解析工具 ----
    std::vector<std::vector<std::uint8_t>> out;
    std::vector<std::uint8_t> raw = {0xAA, 0x55, 0x01, 0x0D, 0x0A,
                                     0xAA, 0x55, 0x02, 0x0D, 0x0A};
    pr::extractFrames(raw.data(), raw.size(), {0xAA, 0x55}, {0x0D, 0x0A}, out);
    std::cout << "  extractFrames 切出 " << out.size() << " 帧\n";

    return 0;
}
