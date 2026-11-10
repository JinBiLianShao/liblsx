# LIBLSX 工具库 Serialize 模块使用说明

---

## 目录

1. [模块概述](#模块概述)
2. [编码约定](#编码约定)
3. [模块架构](#模块架构)
4. [BinaryWriter 使用说明](#binarywriter-使用说明)
5. [BinaryReader 使用说明](#binaryreader-使用说明)
6. [TLV 字段与协议演化](#tlv-字段与协议演化)
7. [与 DataTransfer 的组合模式](#与-datatransfer-的组合模式)
8. [常见错误用法与规避](#常见错误用法与规避)

---

## 模块概述

Serialize 提供轻量二进制编解码工具，面向嵌入式设备间通信协议（配合 DataTransfer
使用）。header-only，零第三方依赖。

核心特性：

* **小端固定宽度**：U8/U16/U32/U64/I*/F32/F64/bool
* **varint（LEB128）**：小数值只占 1 字节；zigzag 变体让小负数同样紧凑
* **字符串/字节数组**：varint 长度前缀，二进制安全（允许内嵌 0x00）
* **TLV 字段**：对齐 protobuf wire format，天然支持未知字段跳过（协议演化）
* **无异常错误处理**：读取失败置 fail 标志，适合解析不可信网络输入

## 编码约定

| 数据 | 编码 |
|------|------|
| 多字节整数 | 小端（与 x86/ARM 内存布局一致） |
| varint | LEB128，每字节低 7 位数据 + 最高位续传标志 |
| 有符号 varint | zigzag 变换后按无符号 varint 编码 |
| 字符串/字节串 | varint 长度前缀 + 原始字节（无 NUL 结尾） |
| TLV 字段 | varint(tag<<3 \| wire_type) + 载荷 |
| TLV 载荷 | Varint=变长；Fixed32/Fixed64=定长无前缀；LengthDelimited=长度前缀 |

跨平台注意：编解码双方必须统一字节序约定。本模块固定输出小端；
大端平台（部分旧网络设备）读取时需自行转换。

---

## 模块架构

```
Include/Serialize/
├── Serialize.h         # BinaryWriter + BinaryReader 全部实现（header-only）
└── lsx_serialize.h     # 便利头文件
```

命名空间：`LSX_LIB::Serialize`。

---

## BinaryWriter 使用说明

```cpp
#include "Serialize/Serialize.h"

LSX_LIB::Serialize::BinaryWriter w(256);   // 预留容量，减少扩容

// 固定宽度整数（小端）
w.writeU8(0xAB);
w.writeU16(0x1234);
w.writeU32(0xDEADBEEF);
w.writeU64(0x0123456789ABCDEFull);
w.writeI32(-123456789);
w.writeF32(25.5f);
w.writeF64(2.718281828);
w.writeBool(true);

// varint（小数值省字节）
w.writeVarint(42);          // 1 字节
w.writeVarintZZ(-1);        // zigzag 有符号，1 字节

// 字符串与字节数组（varint 长度前缀）
w.writeString("hello");
w.writeBytes(raw_data, raw_len);

// TLV 字段（tag + 线路类型 + 载荷）
w.writeField(1, std::uint64_t(123));
w.writeField(2, std::string("value"));

// 取出结果
const auto& bytes = w.data();
w.clear();                   // 复用缓冲区
```

---

## BinaryReader 使用说明

与 BinaryWriter 完全对称。**读取失败不抛异常**：置 fail 标志，后续读取
一律返回零值/空值——适合解析不可信网络输入，最后统一检查。

```cpp
LSX_LIB::Serialize::BinaryReader r(w.data().data(), w.size());

std::uint32_t id = r.readU32();
std::string name = r.readString();
auto value = r.readVarintZZ();

if (r.fail()) {
    // 包损坏/截断/格式错误 —— 统一在此处理
    // r.error() 可区分 OutOfBounds / LengthOverflow / BadWireType
}
```

游标控制：

```cpp
r.pos();        // 当前读取偏移
r.remaining();  // 剩余可读字节数
r.skip(n);      // 跳过 n 字节（越界置 fail）
```

---

## TLV 字段与协议演化

TLV（Tag-Length-Value，对齐 protobuf wire format）是二进制协议向前兼容的
关键：**旧代码遇到新字段能自动跳过**。

```cpp
// v1 写入方
w.writeField(1, std::uint64_t(device_id));
w.writeField(2, std::string(unit));

// v2 写入方新增字段 tag=3
w.writeField(1, std::uint64_t(device_id));
w.writeField(2, std::string(unit));
w.writeField(3, std::uint64_t(firmware_version));   // 新增

// v1 读取方：读到 tag=3 时按 TLV 语义跳过，后续字段不受影响
while (r.remaining() > 0 && !r.fail()) {
    auto f = r.readField();
    switch (f.tag) {
    case 1: device_id = static_cast<std::uint32_t>(f.varint); break;
    case 2: unit.assign(f.bytes.begin(), f.bytes.end()); break;
    default: break;   // 未知字段：忽略即可
    }
}
```

字段类型对应：

| 写入重载 | wire_type | 读取侧 |
|----------|-----------|--------|
| `writeField(tag, uint64)` | Varint | `f.varint` |
| `writeField(tag, int64)` | Varint（zigzag） | `readVarintZZ()` 还原语义 |
| `writeField(tag, string)` | LengthDelimited | `f.bytes` |
| `writeField(tag, WireType::Fixed32, ptr, 4)` | Fixed32 | `f.varint` 低 32 位 |
| `writeField(tag, WireType::Fixed64, ptr, 8)` | Fixed64 | `f.varint` |

---

## 与 DataTransfer 的组合模式

```cpp
// 序列化消息 → TCP 发送
SensorMsg msg{42, 25.5f, "degC"};
BinaryWriter w;
w.writeVarint(0x1001);                 // 消息类型
w.writeField(1, std::uint64_t(msg.device_id));
w.writeField(2, msg.unit);
tcpClient.send(w.data().data(), w.size());

// 接收 → 反序列化
std::uint8_t buf[512];
int n = tcpClient.receive(buf, sizeof(buf));
BinaryReader r(buf, static_cast<std::size_t>(n));
if (r.fail()) { /* 丢弃坏包 */ }
```

与 PacketHeader 组合建议：先写消息类型 varint，再写 TLV 字段——
接收方先读类型再决定如何解析后续字段。

---

## 常见错误用法与规避

### 1. 读写方式不对称

```cpp
// 错误：写用 writeString（带长度前缀），读用 readBytes（也读长度前缀）但语义不同
w.writeString(s);           // varint(len) + bytes
// r.readBytes() 读到的内容正确，但更清晰的对称是 r.readString()

// 正确：写 X 用读 X 读
w.writeString(s);  →  r.readString();
w.writeBytes(v);   →  r.readBytes();
```

### 2. 大端平台直接解释字节

固定宽度整数固定小端输出。在大端平台上收到数据后需要自行字节序转换，
或统一使用 varint（varint 无字节序问题，只有位序约定）。

### 3. 对不可信输入信任长度前缀

```cpp
// readString/readBytes 内部已做长度 vs 剩余字节校验，越界置 fail 返回空
// 但不要在 fail 后继续"信任"后续读取值——统一在最后检查 r.fail()
```

### 4. 混用 wire type

```cpp
// 写入 tag=1 用 Varint，读取却按 LengthDelimited 解析 → BadWireType 或错位
// 约定：同一个 tag 永远使用同一种 wire type（升级版本时保持不变）
```
