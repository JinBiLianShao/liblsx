# LIBLSX 工具库 Checksum 模块使用说明

## 模块概述

Checksum 提供嵌入式通信最常用的校验算法，纯头文件、零依赖、表驱动（constexpr 生成，无全局初始化、无动态分配）。全部为**增量式接口**，适合流式帧校验。命名空间：LSX_LIB::Checksum。

| 算法 | 多项式 / 初值 | 常见用途 |
|------|---------------|----------|
| Crc8 | 0x07 / 0x00 | SMBus、单总线器件 |
| Crc16Modbus | 0xA001 / 0xFFFF | Modbus RTU |
| Crc16CcittFalse | 0x1021 / 0xFFFF | XMODEM、HDLC、蓝牙 |
| Crc32 | 0xEDB88320 / 0xFFFFFFFF | zlib、以太网、ZIP |
| sum8 / xor8 / fletcher16 | 累加/异或/Fletcher | 轻量帧校验 |

## 标准向量（均已测试验证）

| 输入 | 算法 | 结果 |
|------|------|------|
| "123456789" | Crc8 | 0xF4 |
| "123456789" | Crc16CcittFalse | 0x29B1 |
| 01 03 00 00 00 01 | Crc16Modbus | 0x0A84（帧上低字节在前为 0x840A） |
| "123456789" | Crc32 | 0xCBF43926 |

## 使用方式

```cpp
#include "Checksum/Checksum.h"
using namespace LSX_LIB::Checksum;

// 一次性计算
std::uint32_t crc = Crc32::calc(data, len);

// 增量计算（适合分块收到的帧）
Crc16Modbus c;
c.update(part1, n1);
c.update(part2, n2);
std::uint16_t v = c.value();

// 简单校验
std::uint8_t s = sum8(d, n);
std::uint8_t x = xor8(d, n);
std::uint8_t comp = sum8Complement(d, n);   // 整帧含此字节之和为 0
std::uint16_t f = fletcher16(d, n);
```

## API 参考

每个 CRC 类（Crc8 / Crc16Modbus / Crc16CcittFalse / Crc32）都有：

| 方法 | 说明 |
|------|------|
| reset() | 回到初值 |
| update(data, len) | 增量喂入 |
| value() | 当前校验值 |
| static calc(data, len) | 一次性计算 |

Crc16Modbus 额外提供 littleEndian() 返回帧上字节序（低字节在前）的值。

## 与 Protocol 组合（帧校验回调）

```cpp
parser.setCheckCallback([](const std::uint8_t* d, std::size_t n) {
    // 约定：最后一字节是前面内容的 CRC8
    return Crc8::calc(d, n - 1) == d[n - 1];
});
```

## 选型建议

- 与 Modbus 设备通信 → Crc16Modbus
- 通用串口二进制协议 → Crc16CcittFalse 或 Crc8
- 与 PC/文件互操作 → Crc32
- MCU 资源极紧、错误检测要求低 → xor8 / sum8（注意：不能检测偶数个位翻转）
