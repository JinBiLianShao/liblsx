# LIBLSX 工具库 Bus 模块使用说明（嵌入式 Linux）

## 模块概述

Bus 模块封装 Linux 用户态最常用的两种板级总线：**I2C**（/dev/i2c-N）与 **SPI**（/dev/spidevX.Y）。面向嵌入式 Linux，通过标准 ioctl 与字符设备交互，无需内核模块或额外库。

命名空间：LSX_LIB::Bus。

| 组件 | 设备节点 | 底层机制 |
|------|----------|----------|
| I2cBus | /dev/i2c-1 等 | I2C_RDWR ioctl（复合消息） |
| SpiBus | /dev/spidev0.0 等 | SPI_IOC_MESSAGE ioctl（全双工） |

设计要点：RAII 管理 fd；所有操作返回 Result（错误带 errno 说明）；线程安全；**可注入 fd**——构造时传入已打开的 fd 即可在无真实硬件的 CI 中验证上层逻辑。

## 构建与引入

```cmake
target_link_libraries(your_app PRIVATE lsx_static)
```

只需 #include "Bus/Bus.h"。依赖 Linux 内核头（linux/i2c-dev.h、linux/spi/spidev.h），标准嵌入式 Linux 工具链都自带。

## I2cBus 使用说明

```cpp
#include "Bus/Bus.h"

LSX_LIB::Bus::I2cBus i2c("/dev/i2c-1", 0x48);
auto r = i2c.open();
if (r.isErr()) std::cerr << r.error() << std::endl;

auto temp = i2c.readRegister8(0x00);
if (temp.isOk()) std::cout << "寄存器 = " << int(temp.value()) << std::endl;

i2c.writeRegister8(0x01, 0x80);
i2c.writeRegister16(0x1234, 0xAB);
auto v = i2c.readRegister16(0x1234);

std::vector<std::uint8_t> block = i2c.readBlock(0x00, 16).valueOr({});
i2c.writeBlock(0x10, {0x01, 0x02, 0x03});
if (i2c.probe()) { /* 设备在线 */ }
```

### API 参考（I2cBus）

| 方法 | 返回 | 说明 |
|------|------|------|
| open() / close() / isOpen() | Result/void/bool | 设备生命周期 |
| writeRegister8(reg, value) | Result&lt;void&gt; | 8 位地址 + 1 字节 |
| readRegister8(reg) | Result&lt;uint8_t&gt; | 8 位地址读 1 字节 |
| writeRegister16 / readRegister16 | 同上 | 16 位大端地址 |
| readBlock(reg, len) / writeBlock(reg, data) | Result | 连续读写（上限 8192） |
| writeRaw / readRaw | Result | 无寄存器地址裸传输 |
| probe() | bool | 从机探测 |

时序说明：寄存器读写走 I2C_RDWR 复合消息——先写寄存器地址，再读/写数据，中间不插入 STOP，符合绝大多数传感器约定。

## SpiBus 使用说明

```cpp
LSX_LIB::Bus::SpiBus spi("/dev/spidev0.0");
spi.open();
spi.setMode(0); spi.setBitsPerWord(8);
spi.setSpeedHz(1000000); spi.setLsbFirst(false);

auto rx = spi.transfer({0x01, 0x02, 0x03, 0x04});
if (rx.isOk()) { /* rx.value() 是 MISO 同时采到的数据 */ }
spi.write({0xAA, 0xBB});
auto buf = spi.read(16);
```

### API 参考（SpiBus）

| 方法 | 说明 |
|------|------|
| open() / close() / isOpen() | 设备生命周期 |
| setMode(0..3) / mode() | SPI 模式（CPOL/CPHA 组合） |
| setBitsPerWord / setSpeedHz / setLsbFirst | 位宽/时钟/位序配置 |
| transfer(tx) | 全双工，返回 Result&lt;vector&lt;uint8_t&gt;&gt; |
| write(tx) / read(len) | 半双工便捷接口 |

## 可注入 fd（测试用）

```cpp
int fds[2]; socketpair(AF_UNIX, SOCK_STREAM, 0, fds);
LSX_LIB::Bus::I2cBus test_bus(fds[0], 0x50, false);
```

真实设备走 ioctl，注入 fd 走 write/read——上层逻辑一致，CI 无需硬件。

## 常见错误用法与规避

1. **忘记检查 Result**：Err 状态访问 value() 会 assert 失败，用 isOk() 或 valueOr()。
2. **7 位地址写成 8 位**：构造函数要 7 位从机地址（0x03~0x77），不要传含 R/W 位的 8 位值。
3. **16 位寄存器字节序**：本模块固定大端发送地址，小端器件请用 writeRaw/readRaw 自行组织。
4. **SPI 模式不匹配**：mode 0~3 必须与从设备 datasheet 一致，否则数据全错。
