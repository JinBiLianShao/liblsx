# LIBLSX 工具库 Compress 模块使用说明

## 模块概述

Compress 提供 DEFLATE 系压缩与轻量 RLE。DEFLATE 基于 vendor 的 **miniz 3.1.2**（MIT），随库一起编译，目标板无需 zlib 动态库；RLE 为库内自实现，适合 MCU 上重复度高的传感器数据。命名空间：LSX_LIB::Compress。

| 接口 | 格式 | 说明 |
|------|------|------|
| deflate / inflate | RFC 1951 原始 DEFLATE | 最小体积，无头尾 |
| zlibCompress / zlibDecompress | RFC 1950 zlib 流 | 带 2 字节头 + Adler-32 尾 |
| gzipCompress / gzipDecompress | RFC 1952 gzip | 带 10 字节头 + CRC32/ISIZE 尾 |
| rleEncode / rleDecode | 自定义 [value,count] | 极轻量，重复数据专用 |
| crc32 / adler32 | 校验和 | 复用 miniz 实现 |

所有接口失败返回**空 vector**（不抛异常）。压缩级别 Level：NoCompression(0) / Fastest(1) / Default(6) / Best(10)。

## 使用示例

```cpp
#include "Compress/Compress.h"
using namespace LSX_LIB::Compress;

// 原始 DEFLATE
auto c = deflate(data, Level::Best);
auto back = inflate(c, data.size());   // 传期望大小可减少扩容
// 或 auto back = inflate(c);         // 自动扩容

// zlib 流（与 PC 端 zlib 互操作）
auto z = zlibCompress(data);
auto raw = zlibDecompress(z);

// gzip（带 CRC32 校验，损坏可检测）
auto g = gzipCompress(data);
auto out = gzipDecompress(g);

// RLE（重复度高的 ADC 采样）
auto r = rleEncode(samples);
auto s = rleDecode(r);
```

## 实现要点

- **inflate 自动扩容**：miniz 的 MZ_FINISH 在输出缓冲扩容后无法续解，本模块用 MZ_NO_FLUSH 循环驱动，输出满自动翻倍（上限 64MB），输入耗尽才结束。
- **zlib 尾部校验**：zlib 流必须等 MZ_STREAM_END 才算完成（Adler-32 校验），不能因输入耗尽提前退出。
- **gzip 完整性**：解压后校验 CRC32 与 ISIZE，任一不符返回空。
- **RLE 边界**：count 上限 255，count=0 或奇数长度输入拒绝；不重复数据会膨胀（最差 2 倍）。

## 选型建议

| 场景 | 推荐 |
|------|------|
| 与 PC/网络互操作 | zlib / gzip |
| 板内存储、追求最小体积 | 原始 deflate |
| MCU 无堆、重复度高的数据 | RLE |
| 需要校验压缩完整性 | gzip（自带 CRC32）或 zlib（Adler-32） |

## 与 Serialize / DataTransfer 组合

压缩前先序列化：Serialize 输出 → zlibCompress → DataTransfer 发送；接收端 zlibDecompress → BinaryReader 解码。gzip 尾部的 CRC32 还能顺带做传输完整性校验。
