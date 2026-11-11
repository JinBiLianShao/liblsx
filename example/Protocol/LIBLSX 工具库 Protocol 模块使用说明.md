# LIBLSX 工具库 Protocol 模块使用说明

## 模块概述

Protocol 提供通用**流式帧解析**：从串口/总线收到的杂乱字节流中，按"帧头 + 帧尾"或"帧头 + 长度字段"切出完整帧，天然处理粘包与半包。

命名空间：LSX_LIB::Protocol。header + 单个 .cpp，内部固定缓冲无动态分配。

| 组件 | 用途 |
|------|------|
| FrameParser | 状态机驱动的逐字节帧解析器 |
| extractFrames() | 无状态批量提帧 |

## 快速上手

```cpp
#include "Protocol/FrameParser.h"

LSX_LIB::Protocol::FrameParser parser(4096);
parser.setHeader({0xAA, 0x55});
parser.setEnder({0x0D, 0x0A});
parser.onFrame([](const std::uint8_t* frame, std::size_t len) {
    const std::uint8_t* content; std::size_t n;
    if (LSX_LIB::Protocol::FrameParser::contentOf(frame, len, 2, 2, &content, &n))
        handle(content, n);
});
for (auto b : recv_bytes) parser.feed(b);
```

## 帧配置方式

**帧头 + 帧尾**：setHeader({0xAA,0x55}) + setEnder({0x0D,0x0A})。帧头部分匹配会自动回退：流里出现 AA AA 55 仍能正确命中 AA 55。

**帧头 + 长度字段**（变长帧，无帧尾）：setLengthField(0, 1) 表示帧头后第 0 字节起、1 字节小端长度；长度值表示"长度字段之后的字节数"，支持 1/2/4 字节小端。

**转义字节**：setEscapeByte(0x1B)，载荷中出现帧尾字节时先发转义字节；转义字节本身不入帧，其后一字节原样入帧且不参与帧尾判断。

**校验回调**：setCheckCallback(cb) 收到帧头之后、帧尾之前的内容，返回 false 则丢弃该帧。可直接配合 Checksum 模块。

## 状态机与边界行为

| 场景 | 行为 |
|------|------|
| 帧头前噪声 | 忽略直到匹配帧头 |
| 帧头部分匹配失配 | 回退重新匹配，不吞字节 |
| 半包（分多次 feed） | 缓冲保留，补齐后成帧 |
| 粘包（一次多帧） | 连续触发多次 onFrame |
| 超长帧 | 丢弃、重置、rejected+1 |
| 校验失败 | 丢弃、rejected+1 |

回调在 feed() 调用者线程同步执行、**锁外分发**——回调里可安全再次 feed/reset。

## API 参考

| 方法 | 说明 |
|------|------|
| setHeader / setEnder | 帧头帧尾（最多 8 字节） |
| setEscapeByte / disableEscape | 转义配置 |
| setLengthField(offset, bytes) | 长度字段模式 |
| setCheckCallback / onFrame | 校验与成帧回调 |
| feed(byte) / feed(data, len) | 喂入字节流 |
| reset / pending / framesAccepted / framesRejected | 状态与统计 |
| static contentOf(...) | 去掉头尾取内容区间 |

批量提帧：extractFrames(data, len, header, ender, frames) 返回末尾未成帧的字节数，保留到下一批继续。

## 与 Serialize / Checksum 组合

```cpp
parser.setCheckCallback([](const std::uint8_t* d, std::size_t n) {
    return Checksum::xor8(d, n - 1) == d[n - 1];
});
parser.onFrame([](const std::uint8_t* f, std::size_t n) {
    const std::uint8_t* body; std::size_t len;
    FrameParser::contentOf(f, n, 2, 2, &body, &len);
    Serialize::BinaryReader r(body, len);
});
```

## 常见错误用法与规避

1. 回调里做重活会阻塞 feed 调用方，重逻辑投递线程池。
2. 粘包会触发多次回调，回调要能重入。
3. 帧尾字节出现在载荷里时启用转义，或改用长度字段模式。
4. UDS/TCP 是字节流协议，必须靠本模块分帧，不要假设一次 receive 就是一帧。
