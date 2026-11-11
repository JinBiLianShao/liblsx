/**
 * @file FrameParser.cpp
 * @brief FrameParser 实现
 */
#include "Protocol/FrameParser.h"

#include <algorithm>
#include <cstring>

namespace LSX_LIB {
namespace Protocol {

void FrameParser::feed(std::uint8_t b)
{
    if (state_ == State::SeekHeader)
    {
        // 帧头匹配：允许部分匹配后回退（例如帧头 AA 55，收到 AA AA 55 仍能命中）
        if (!header_.empty())
        {
            if (b == header_[matched_])
            {
                buf_[buf_len_++] = b;
                ++matched_;
                if (matched_ == header_.size())
                {
                    state_ = State::InFrame;
                    matched_ = 0;
                    expected_total_ = 0;
                }
            }
            else
            {
                // 不匹配：重新从当前字节开始匹配（处理 AA AA 55 这类重叠情形）
                matched_ = (b == header_[0]) ? 1u : 0u;
                buf_len_ = matched_;
                if (matched_ > 0) buf_[0] = b;
            }
            return;
        }
        // 无帧头配置：任何字节都算帧开始
        state_ = State::InFrame;
        buf_[buf_len_++] = b;
        return;
    }

    // ---- InFrame / Escaped ----
    if (state_ == State::Escaped)
    {
        // 转义字节后的一个字节原样入帧，不参与帧尾/长度判断
        state_ = State::InFrame;
        if (buf_len_ < max_frame_) buf_[buf_len_++] = b;
        else { reset(); }
        return;
    }

    if (use_escape_ && b == escape_)
    {
        state_ = State::Escaped;
        return;   // 转义字节本身不入帧
    }

    if (buf_len_ >= max_frame_)
    {
        // 超长保护：丢弃当前帧重新找头，避免缓冲区溢出
        ++rejected_;
        reset();
        return;
    }
    buf_[buf_len_++] = b;

    if (use_length_)
    {
        if (expected_total_ == 0 && buf_len_ >= header_.size() + len_offset_ + len_bytes_)
            expected_total_ = header_.size() + len_offset_ + len_bytes_ + declaredLength();
        if (expected_total_ != 0 && buf_len_ >= expected_total_)
            emit(runCheck());
        return;
    }

    if (!ender_.empty() && buf_len_ >= header_.size() + ender_.size() && matchesEnder())
    {
        emit(runCheck());
    }
}

bool FrameParser::matchesEnder() const
{
    return std::memcmp(buf_.data() + buf_len_ - ender_.size(),
                       ender_.data(), ender_.size()) == 0;
}

std::size_t FrameParser::declaredLength() const
{
    std::size_t v = 0;
    std::size_t off = header_.size() + len_offset_;
    for (std::size_t i = 0; i < len_bytes_ && off + i < buf_len_; ++i)
        v |= static_cast<std::size_t>(buf_[off + i]) << (i * 8);  // 小端
    return v;
}

bool FrameParser::runCheck() const
{
    if (!check_cb_) return true;   // 未配置校验 → 视为通过
    const std::uint8_t* content = buf_.data() + header_.size();
    std::size_t content_len = buf_len_ - header_.size();
    if (use_length_)
    {
        // 校验回调期望收到"不含长度字段之外的载荷"——此处给全部内容
        return check_cb_(content, content_len);
    }
    if (content_len >= ender_.size()) content_len -= ender_.size();
    return check_cb_(content, content_len);
}

void FrameParser::emit(bool valid)
{
    if (!valid)
    {
        ++rejected_;
        reset();
        return;
    }
    ++accepted_;
    if (on_frame_) on_frame_(buf_.data(), buf_len_);
    reset();
}

std::size_t extractFrames(const std::uint8_t* data, std::size_t len,
                          const std::vector<std::uint8_t>& header,
                          const std::vector<std::uint8_t>& ender,
                          std::vector<std::vector<std::uint8_t>>& out)
{
    FrameParser p(len + 16);
    p.setHeader(header);
    p.setEnder(ender);
    p.onFrame([&out](const std::uint8_t* f, std::size_t n) {
        out.emplace_back(f, f + n);
    });
    p.feed(data, len);
    return p.pending();
}

} // namespace Protocol
} // namespace LSX_LIB
