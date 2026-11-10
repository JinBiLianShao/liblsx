/**
 * @file test_framework.h
 * @brief LIBLSX 测试套件 —— 极简测试框架
 * 提供 TEST(套件名, 用例名) 注册宏、断言宏与统一运行入口。
 */
#pragma once

#include <cstdio>
#include <cstdlib>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>
#include <exception>

namespace lsx_test {

struct TestCase {
    std::string suite;
    std::string name;
    std::function<void()> fn;
};

inline std::vector<TestCase>& registry() {
    static std::vector<TestCase> r;
    return r;
}

inline int& failure_count() {
    static int c = 0;
    return c;
}

struct Registrar {
    Registrar(const std::string& suite, const std::string& name, std::function<void()> fn) {
        registry().push_back({suite, name, std::move(fn)});
    }
};

inline int run_all() {
    int passed = 0, failed = 0;
    for (const auto& tc : registry()) {
        std::printf("[ RUN  ] %s.%s\n", tc.suite.c_str(), tc.name.c_str());
        std::fflush(stdout);
        try {
            tc.fn();
            ++passed;
            std::printf("[ PASS ] %s.%s\n", tc.suite.c_str(), tc.name.c_str());
        } catch (const std::exception& e) {
            ++failed; ++failure_count();
            std::printf("[ FAIL ] %s.%s —— 异常: %s\n", tc.suite.c_str(), tc.name.c_str(), e.what());
        } catch (...) {
            ++failed; ++failure_count();
            std::printf("[ FAIL ] %s.%s —— 未知异常\n", tc.suite.c_str(), tc.name.c_str());
        }
        std::fflush(stdout);
    }
    std::printf("\n========== 测试汇总: %d 通过, %d 失败, 共 %d ==========\n", passed, failed, passed + failed);
    return failed == 0 ? 0 : 1;
}

} // namespace lsx_test

#define LSX_TEST(suite, name) \
    static void lsx_test_##suite##_##name(); \
    static lsx_test::Registrar lsx_reg_##suite##_##name(#suite, #name, lsx_test_##suite##_##name); \
    static void lsx_test_##suite##_##name()

#define LSX_EXPECT_TRUE(cond) \
    do { if (!(cond)) { std::printf("    断言失败: %s (%s:%d)\n", #cond, __FILE__, __LINE__); throw std::runtime_error(#cond); } } while (0)

#define LSX_EXPECT_EQ(a, b) \
    do { if (!((a) == (b))) { std::printf("    断言失败: %s == %s (%s:%d)\n", #a, #b, __FILE__, __LINE__); throw std::runtime_error(std::string(#a) + " == " + #b); } } while (0)

#define LSX_EXPECT_NEAR(a, b, eps) \
    do { if (!((a) > (b) - (eps) && (a) < (b) + (eps))) { std::printf("    断言失败: %s ≈ %s (%s:%d)\n", #a, #b, __FILE__, __LINE__); throw std::runtime_error(std::string(#a) + " ≈ " + #b); } } while (0)
