# LIBLSX 深度测试套件使用说明

## 概述

tests/ 是 LIBLSX 的**自包含深度测试套件**：零第三方依赖、无 GoogleTest/Catch2，由库内自带的极简测试框架驱动。当前共 **257 个用例**，覆盖全部 17 个功能模块。

| 文件 | 职责 |
|------|------|
| test_framework.h | 测试框架：注册宏、断言宏、运行入口 |
| test_main.cpp | main()：仅调用 lsx_test::run_all() |
| test_*.cpp | 各模块测试用例（按模块/主题分文件） |
| CMakeLists.txt | 构建目标 lsx_tests + CTest 注册 + run_tests 自定义目标 |
| README.md | 本文档 |

## 快速运行

### 标准方式（推荐）

    mkdir -p build && cd build
    cmake .. -DCOMPILER_TARGET=x86 -DTARGET_TYPE=both -DENABLE_TESTS=ON
    make -j
    ctest --output-on-failure

### 直接运行可执行文件

    ./x86/bin/lsx_tests            # 全部用例
    ./x86/bin/lsx_tests | tail -5  # 只看汇总

### 使用自定义目标

    make run_tests                 # 等价于运行 lsx_tests

### 交叉编译平台说明

    # 测试目标为 x86 主机二进制，交叉编译（arm32/arm64）只构建库本身；
    # 测试需在本机 x86 构建目录里跑。
    cmake .. -DCOMPILER_TARGET=x86 -DENABLE_TESTS=ON && make -j && ctest

ENABLE_TESTS 默认为 OFF，开启后才会进入 tests/ 子目录编译测试目标。CTest 用例名为 lsx_deep_tests，超时上限 120 秒。

## 输出格式

    [ RUN  ] Suite.CaseName
    [ PASS ] Suite.CaseName
    [ FAIL ] Suite.CaseName —— 异常: 断言失败原因
    ...
    ========== 测试汇总: 257 通过, 0 失败, 共 257 ==========

- 每个用例抛异常即判 FAIL（框架捕获 std::exception 与 ... 两种）
- 进程退出码：全部通过返回 0，有失败返回 1（可直接用于 CI 判定）
- 汇总行给出通过/失败/总数

## 编写新用例

### 最小示例

新建 tests/test_mymodule.cpp：

    #include "test_framework.h"
    #include "MyModule/MyHeader.h"

    LSX_TEST(MyModule, BasicBehavior) {
        MyType obj;
        LSX_EXPECT_TRUE(obj.isValid());
        LSX_EXPECT_EQ(obj.value(), 42);
        LSX_EXPECT_NEAR(obj.ratio(), 1.5, 1e-9);
    }

**无需手写 main、无需注册列表、无需改 CMake**——LSX_TEST 宏通过静态 Registrar 自动把用例登记进全局 registry，CMakeLists.txt 用 file(GLOB *.cpp) 自动收集新文件。

### 宏参考

| 宏 | 语义 |
|----|------|
| LSX_TEST(suite, name) | 定义一个用例；suite/name 必须是合法 C++ 标识符（用于拼接内部函数名，不能含空格/点） |
| LSX_EXPECT_TRUE(cond) | cond 为假则输出断言位置并抛异常 |
| LSX_EXPECT_EQ(a, b) | a == b，否则失败（打印表达式文本） |
| LSX_EXPECT_NEAR(a, b, eps) | a 落在 b ± eps 内 |

失败信息包含表达式文本、__FILE__、__LINE__，便于定位。

### 断言之外的检查

框架只提供三个断言宏，其余用标准 C++ 组合即可：

    LSX_EXPECT_TRUE(std::string(reinterpret_cast<char*>(buf), n) == "hello");
    LSX_EXPECT_TRUE(a != b);
    LSX_EXPECT_TRUE(vec.empty());

复杂条件建议先算出布尔量再 LSX_EXPECT_TRUE，可读性更好。

## 用例命名约定

- suite 用模块名或类名：ThreadWrapper、Serialize、ConfigStore、StateMachine
- name 用 CamelCase 描述行为：OnceFiresAfterDelay、GuardRejectedRecordedInHistory
- 回归缺陷的用例名带语义提示：PeriodicFiresRepeatedly、CircularQueuePeekAfterV06Fix
- 一个文件内 suite.name 组合不可重复（宏会生成同名静态函数）

## 测试编写规范（本库实战沉淀）

1. **每个用例必须自洽**：不依赖其它用例的执行顺序或残留状态（registry 顺序按静态初始化，跨编译单元不保证）。
2. **临时文件用唯一路径**：文件名带 getpid()，避免并行/重复运行冲突；用完 std::remove。
3. **并发用例必须无死锁退出**：用 std::atomic&lt;bool&gt; stop 控制观察线程，配合 sleep_for 退避；不要用无限 yield() 空转抢锁（曾因此伪装成库死锁）。
4. **阻塞 IO 一律设超时**：socket/UDS/串口用例先 setReceiveTimeout(ms)，否则 CI 会挂死。
5. **检查所有返回值**：Enqueue/send/create 失败时静默返回，不检查会伪装成"丢数据"。
6. **用已知标准向量**：CRC32 "123456789"、FIPS-197 AES、SHA-256 "abc"、RFC 4231 HMAC 等验证算法正确性，不只用回环。
7. **损坏输入用例**：解析器/解码器必须测截断、非法字符、长度溢出，确认不崩溃且有确定的失败标志。
8. **无硬件可测**：Bus 模块用"可注入 fd" + socketpair 回显；真实 /dev 设备的 ioctl 路径留给目标板集成测试。
9. **头文件自包含回归**：test_interfaces.cpp 同时 include 全部便利头，验证跨模块 include 路径修复不回潮。

## 扩展方式

### 加一个新模块的测试

1. 新建 tests/test_&lt;module&gt;.cpp，include test_framework.h + 模块头
2. 写 LSX_TEST(...) 用例（参考上面的最小示例）
3. cmake .. -DENABLE_TESTS=ON && make -j && ctest（GLOB 自动收集，新增文件后需重新 cmake 一次以刷新 GLOB）

### 加新的断言宏

编辑 tests/test_framework.h，仿照 LSX_EXPECT_TRUE 加宏定义。断言宏必须**抛异常**（框架靠异常判失败），不能直接 abort()。例如：

    #define LSX_EXPECT_THROW(expr, ExType)         do { bool caught = false;              try { (void)(expr); } catch (const ExType&) { caught = true; }              if (!caught) { std::printf("    断言失败: %s 应抛异常
", #expr);                             throw std::runtime_error(#expr); } } while (0)

### 调整 CTest 超时

tests/CMakeLists.txt 中 set_tests_properties(lsx_deep_tests PROPERTIES TIMEOUT 120)；用例增多时可上调。

## 测试文件与用例数（v0.7.0）

| 文件 | 用例数 | 主要覆盖 |
|------|--------|----------|
| test_thread.cpp | 16 | ThreadWrapper/Scheduler/ThreadPool |
| test_timer.cpp | 18 | TimerWheel/Stopwatch/Deadline |
| test_datatransfer.cpp | 10 | TCP/UDP/串口/工厂 |
| test_lock.cpp | 9 | 全锁类型/Condition/MultiLockGuard |
| test_memory.cpp | 14 | Buffer/Pipe/FIFO/队列/SharedMemory |
| test_logger.cpp | 4 | 级别/轮转/并发 |
| test_eventbus.cpp | 12 | pub/sub/句柄/嵌套 |
| test_serialize.cpp | 20 | varint/TLV/损坏输入 |
| test_ipc.cpp | 9 | UDS 回环/超时/断线 |
| test_result.cpp | 12 | Result/void/map |
| test_bus.cpp | 17 | I2C/SPI 注入 fd |
| test_protocol.cpp | 13 | 帧解析/粘包/转义/校验 |
| test_checksum.cpp | 10 | CRC 标准向量 |
| test_crypto.cpp | 19 | AES/SHA/HMAC/编码 |
| test_compress.cpp | 14 | DEFLATE/zlib/gzip/RLE |
| test_config.cpp | 6 | INIReader |
| test_configstore.cpp | 18 | ConfigStore 绑定/回调/并发 |
| test_interfaces.cpp | 6 | 头文件自包含/接口回归 |
| test_gapfill.cpp | 9 | 未覆盖类补盲 |
| test_statemachine.cpp | 21 | 泛型 FSM/层次/超时/重入 |
| **合计** | **257** | 17 个模块 |

## 常见问题

**Q：加了 test_xxx.cpp 但没被执行？**
A：file(GLOB) 在首次 cmake 时固化，新增源文件后重新跑一次 cmake .. -DENABLE_TESTS=ON 即可。

**Q：用例挂死（超时 120s）？**
A：多半是缺少超时设置的阻塞 IO 或观察线程没退出。按上面"测试编写规范"第 3/4 条检查；可用 timeout 30 ./x86/bin/lsx_tests 快速定位停在哪个 [ RUN ]。

**Q：如何只跑某个模块的用例？**
A：框架目前不支持按 suite 过滤（保持极简）。临时调试可注释掉其它 test_*.cpp，或在 test_main.cpp 里给 run_all() 加 suite 过滤参数后重新编译。

**Q：CI 里怎么用？**
A：cmake -DENABLE_TESTS=ON && make -j && ctest --output-on-failure；退出码非 0 即失败，无需解析输出。三轮稳定性验证做法是连续跑三次全绿。
