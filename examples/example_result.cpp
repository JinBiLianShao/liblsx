/**
 * @file example_result.cpp
 * @brief LIBLSX Result 模块使用示例
 *
 * 演示内容：
 *   1. Result<T, E>：值或错误二者取一
 *   2. valueOr：失败给默认值
 *   3. map：成功时链式变换
 *   4. void 特化：只关心成败
 *
 * 关键点：
 *   - Err 状态访问 value() 会 assert（fail fast，暴露编程错误）；
 *     不想 assert 就用 valueOr / isOk 分支。
 */

#include "Result/Result.h"

#include <iostream>
#include <string>

namespace rs = LSX_LIB::Result;

// 一个"可能失败"的解析函数：返回 Result<int, std::string>
static rs::Result<int, std::string> parse_port(const std::string& s)
{
    if (s == "8080") return rs::Ok(8080);
    return rs::Err(std::string("无法解析: ") + s);
}

int main()
{
    std::cout << "===== Result 统一错误处理示例 =====\n";

    // ---- 1) 值/错误二者取一 ----
    auto ok = parse_port("8080");
    if (ok.isOk()) {
        std::cout << "  解析成功: " << ok.value() << "\n";
    } else {
        std::cout << "  解析失败: " << ok.error() << "\n";
    }

    auto bad = parse_port("abc");
    if (bad.isErr()) {
        std::cout << "  失败信息: " << bad.error() << "\n";
    }

    // ---- 2) valueOr：失败给默认值（不 assert）----
    int port = parse_port("bad").valueOr(8080);
    std::cout << "  valueOr 默认值: " << port << "\n";

    // ---- 3) map：成功时变换，失败原样传递 ----
    auto doubled = parse_port("8080").map([](int v) { return v * 2; });
    std::cout << "  map 后: " << (doubled.isOk() ? doubled.value() : -1) << "\n";

    // ---- 4) void 特化：只关心成败 ----
    rs::Result<void, std::string> save_result = rs::Err(std::string("磁盘已满"));
    if (save_result.isErr()) {
        std::cout << "  保存失败: " << save_result.error() << "\n";
    }

    return 0;
}
