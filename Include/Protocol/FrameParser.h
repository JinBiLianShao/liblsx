/**
 * @file FrameParser.h
 * @brief LIBLSX 工具库 - 通用协议帧解析模块
 * @details 提供 LSX_LIB::Protocol 命名空间下的流式帧提取工具：
 *  - FrameParser：可配置的"帧头/帧尾/长度字段/转义/校验"解析器，
 *    逐字节喂入（feed）即可从杂乱字节流中切出完整帧，天然处理粘包/半包。
 *  - 预置常用协议模板：Modbus RTU 风格、定长帧、带长度字段的变长帧。
 *
 * 设计面向嵌入式串口/总线字节流：无动态分配（内部固定环形缓冲）、
 * 状态机驱动、支持帧内转义字节与校验回调。
 *
 * @author 连思鑫（jinbilianshao）
 * @date 2026-11-11
 * @version 1.0
 *
 * ### 使用示例
 * @code
 * #include "Protocol/FrameParser.h"
 *
 * LSX_LIB::Protocol::FrameParser parser;
 * parser.setHeader({0xAA, 0x55});
 * parser.setEnder({0x0D, 0x0A});
 * parser.onFrame([](const std::uint8_t* frame, std::size_t len) {
 *     handle(frame, len);   // frame 含帧头帧尾的完整帧
 * });
 *
 * // 串口收到数据后逐字节喂入
 * for (auto b : recv_bytes) parser.feed(b);
 * @endcode
 
 *
 * ### 线程安全 (@thread_safety)
 * FrameParser 实例不可多线程共享（内部缓冲无锁）；回调在 feed() 调用者线程同步执行。
 */
#pragma once

#include <utility>
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

#include "Checksum/Checksum.h"

namespace LSX_LIB {
namespace Protocol {

/// 帧校验结果
enum class CheckResult : std::uint8_t
{
    Ok = 0,        ///< 校验通过
    Failed,        ///< 校验失败
    NotConfigured  ///< 未配置校验
};

/**
 * @class FrameParser
 * @brief 通用流式帧解析器（帧头帧尾 + 可选长度/转义/校验）
 *
 * 状态机：SEEK_HEADER → IN_FRAME → (ESC) → COMPLETE。
 * 帧头匹配失败时回退一个字节重新匹配，避免"帧头中含部分匹配"被吞掉。
 * 输出帧包含帧头与帧尾（可通过 frameContent() 去掉）。
 */
class FrameParser
{
public:
    static constexpr std::size_t kMaxHeader = 8;
    static constexpr std::size_t kMaxEnder = 8;

    using FrameCallback = std::function<void(const std::uint8_t*, std::size_t)>;
    using CheckCallback = std::function<bool(const std::uint8_t*, std::size_t)>;

    explicit FrameParser(std::size_t max_frame = 4096)
        : buf_(max_frame), max_frame_(max_frame) {}

    // ---- 配置 ----

    void setHeader(std::vector<std::uint8_t> h) { header_ = std::move(h); }
    void setEnder(std::vector<std::uint8_t> e) { ender_ = std::move(e); }
    void setEscapeByte(std::uint8_t esc) { escape_ = esc; use_escape_ = true; }
    void disableEscape() { use_escape_ = false; }

    /// 帧尾校验回调：入参为"帧头与帧尾之间"的内容（不含头尾）
    void setCheckCallback(CheckCallback cb) { check_cb_ = std::move(cb); }

    /// 变长帧：长度字段从帧头之后第 len_offset 字节开始，占 len_bytes 字节（小端），
    /// 长度值表示"帧头+长度字段之后的剩余字节数"（含载荷与帧尾）。设置后不依赖帧尾。
    void setLengthField(std::size_t len_offset, std::size_t len_bytes)
    {
        // len_bytes 仅支持 1/2/4（小端整型字段），其它值会在 declaredLength() 里造成位移 UB
        if (len_bytes != 1 && len_bytes != 2 && len_bytes != 4) return;
        use_length_ = true;
        len_offset_ = len_offset;
        len_bytes_ = len_bytes;
    }

    void onFrame(FrameCallback cb) { if (cb) on_frame_ = std::move(cb); }

    // ---- 喂入 ----

    /// 逐字节喂入；每切出一帧调用一次 onFrame 回调
    void feed(std::uint8_t b);

    /// 批量喂入
    void feed(const std::uint8_t* data, std::size_t len)
    {
        for (std::size_t i = 0; i < len; ++i) feed(data[i]);
    }

    void reset() { buf_len_ = 0; state_ = State::SeekHeader; matched_ = 0; }

    /// 已缓存但尚未成帧的字节数（调试用）
    std::size_t pending() const { return buf_len_; }

    /// 去掉帧头帧尾后的内容区间（配合 onFrame 使用）
    static bool contentOf(const std::uint8_t* frame, std::size_t len,
                          std::size_t header_len, std::size_t ender_len,
                          const std::uint8_t** out, std::size_t* out_len)
    {
        if (len < header_len + ender_len) return false;
        *out = frame + header_len;
        *out_len = len - header_len - ender_len;
        return true;
    }

    /// 帧统计
    std::size_t framesAccepted() const { return accepted_; }
    std::size_t framesRejected() const { return rejected_; }

private:
    enum class State : std::uint8_t { SeekHeader, InFrame, Escaped };

    void emit(bool valid);
    bool matchesEnder() const;
    bool runCheck() const;
    std::size_t declaredLength() const;

    std::vector<std::uint8_t> header_;
    std::vector<std::uint8_t> ender_;
    std::uint8_t escape_ = 0;
    bool use_escape_ = false;
    bool use_length_ = false;
    std::size_t len_offset_ = 0;
    std::size_t len_bytes_ = 1;
    CheckCallback check_cb_;
    FrameCallback on_frame_;

    std::vector<std::uint8_t> buf_;
    std::size_t max_frame_;
    std::size_t buf_len_ = 0;
    State state_ = State::SeekHeader;
    std::size_t matched_ = 0;
    std::size_t accepted_ = 0;
    std::size_t rejected_ = 0;
    std::size_t expected_total_ = 0;   // 长度字段模式下的整帧长度
};

/**
 * @brief 一次性从缓冲区中提取所有完整帧（无状态，适合已收齐一批数据的场景）
 * @param data 输入字节流
 * @param len 输入长度
 * @param header 帧头
 * @param ender 帧尾
 * @param out 输出：每个元素为一帧（含帧头帧尾）
 * @return 剩余未成帧的尾部字节数（调用方可保留到下一批）
 */
std::size_t extractFrames(const std::uint8_t* data, std::size_t len,
                          const std::vector<std::uint8_t>& header,
                          const std::vector<std::uint8_t>& ender,
                          std::vector<std::vector<std::uint8_t>>& out);

} // namespace Protocol
} // namespace LSX_LIB
