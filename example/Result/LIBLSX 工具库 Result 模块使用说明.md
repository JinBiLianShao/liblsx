# LIBLSX 工具库 Result 模块使用说明

---

## 目录

1. [模块概述](#模块概述)
2. [快速上手](#快速上手)
3. [API 参考](#api-参考)
4. [void 特化](#void-特化)
5. [与其它错误风格的对比](#与其它错误风格的对比)
6. [常见错误用法与规避](#常见错误用法与规避)

---

## 模块概述

Result<T, E> 表示"值或错误的二者取一"，是 C++17 缺失的 std::expected 过渡方案。
目标是统一错误处理风格：库内不再混用 bool 返回 + 输出参数、魔法返回值、异常。

核心特性：

* **类型安全**：成功值与错误信息都是一等公民，编译期强制检查
* **header-only**，零依赖
* **fail fast**：Err 状态访问 value() 会触发 assert（Debug 构建立即暴露编程错误）
* **valueOr**：失败给默认值的一行写法
* **map**：成功链式变换，错误自动传播
* **void 特化**：只关心成败的接口

---

## 快速上手

```cpp
#include "Result/Result.h"

using LSX_LIB::Result::Result;
using LSX_LIB::Result::Ok;
using LSX_LIB::Result::Err;

Result<int, std::string> parse(const std::string& s) {
    if (s == "42") return Ok(42);
    return Err("bad input: " + s);
}

auto r = parse("42");
if (r.isOk()) {
    std::cout << r.value();       // 42
} else {
    std::cerr << r.error();
}

// 失败给默认值（一行）
int port = parse("80").valueOr(8080);
```

---

## API 参考

| 方法 | 说明 |
|------|------|
| `Result(OkTag<T>)` / `Ok(v)` | 构造成功分支 |
| `Result(ErrTag<E>)` / `Err(e)` | 构造失败分支 |
| `bool isOk() / isErr()` | 状态检查 |
| `const T& value()` | 取值（Err 状态 assert 失败） |
| `const E& error()` | 取错误（Ok 状态 assert 失败） |
| `T valueOr(T fallback)` | 取值，失败给默认值 |
| `auto map(Fn) const` | Ok 时变换值，Err 时传播错误 |

---

## void 特化

只关心成败的接口使用 `Result<void>`：

```cpp
Result<void> save_config() {
    if (!write_file()) return Err("write failed");
    return {};              // 默认构造 = 成功
}

if (save_config().isErr()) {
    // 处理失败
}
```

---

## 与其它错误风格的对比

| 风格 | 问题 | Result 的解法 |
|------|------|--------------|
| bool + 输出参数 | 调用方容易漏检查，无法携带错误详情 | 错误是返回值一部分，编译期可见 |
| 魔法返回值（-1/null） | 与合法值冲突，需查文档 | 状态与值分离，无歧义 |
| 异常 | 嵌入式常禁用异常；控制流不可见 | 错误显式传递，零异常开销 |

---

## 常见错误用法与规避

### 1. Err 状态访问 value()

Debug 构建 assert 立即失败（fail fast）；Release 构建行为未定义。
访问前必须用 isOk()/isErr() 分支，或使用 valueOr() 提供默认值。

### 2. 忽略返回的 Result

```cpp
save_config();            // 错误被静默丢弃
// 建议：
if (!save_config().isOk()) { /* 处理 */ }
```

### 3. 把 Result 当 optional 用

Result 的 Err 应承载"为什么失败"的信息。只想表达"有没有"时，
std::optional 语义更轻（本库 Memory 模块的 Pop/Dequeue 即是）。
