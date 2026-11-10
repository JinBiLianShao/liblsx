/**
 * @file test_result.cpp
 * @brief Result 模块深度测试：语义/值语义/void 特化/map/valueOr
 */
#include "test_framework.h"

#include <memory>
#include <string>
#include <utility>

#include "Result/Result.h"

using namespace LSX_LIB;

LSX_TEST(Result, OkCarriesValue) {
    auto r = Result::Result<int, std::string>(Result::Ok(42));
    LSX_EXPECT_TRUE(r.isOk());
    LSX_EXPECT_TRUE(!r.isErr());
    LSX_EXPECT_EQ(r.value(), 42);
}

LSX_TEST(Result, ErrCarriesError) {
    auto r = Result::Result<int, std::string>(Result::Err(std::string("boom")));
    LSX_EXPECT_TRUE(r.isErr());
    LSX_EXPECT_EQ(r.error(), std::string("boom"));
}

LSX_TEST(Result, DefaultErrorTypeIsString) {
    auto r = Result::Result<int>(Result::Err(std::string("default E")));
    LSX_EXPECT_EQ(r.error(), std::string("default E"));
}

LSX_TEST(Result, ValueOrReturnsFallbackOnErr) {
    auto ok = Result::Result<int>(Result::Ok(7));
    LSX_EXPECT_EQ(ok.valueOr(0), 7);

    auto err = Result::Result<int>(Result::Err(std::string("x")));
    LSX_EXPECT_EQ(err.valueOr(0), 0);
}

LSX_TEST(Result, MapTransformsValue) {
    auto r = Result::Result<int>(Result::Ok(21))
                 .map([](int v) { return v * 2; });
    LSX_EXPECT_TRUE(r.isOk());
    LSX_EXPECT_EQ(r.value(), 42);
}

LSX_TEST(Result, MapSkippedOnErr) {
    auto r = Result::Result<int>(Result::Err(std::string("skip me")))
                 .map([](int v) { return v * 2; });
    LSX_EXPECT_TRUE(r.isErr());
    LSX_EXPECT_EQ(r.error(), std::string("skip me"));
}

LSX_TEST(Result, VoidSpecializationSuccess) {
    Result::Result<void> r;   // 默认构造 = 成功
    LSX_EXPECT_TRUE(r.isOk());
    LSX_EXPECT_TRUE(!r.isErr());
}

LSX_TEST(Result, VoidSpecializationFailure) {
    Result::Result<void> r = Result::Err(std::string("io error"));
    LSX_EXPECT_TRUE(r.isErr());
    LSX_EXPECT_EQ(r.error(), std::string("io error"));
}

LSX_TEST(Result, VoidOkHelper) {
    auto r = Result::OkVoid();
    LSX_EXPECT_TRUE(r.isOk());
}

LSX_TEST(Result, CopyAndMoveSemantics) {
    auto r1 = Result::Result<std::shared_ptr<int>>(Result::Ok(std::make_shared<int>(5)));
    auto r2 = r1;   // 拷贝：shared_ptr 引用共享
    LSX_EXPECT_EQ(*r2.value(), 5);
    *r1.value() = 6;
    LSX_EXPECT_EQ(*r2.value(), 6);   // 同一对象

    auto r3 = std::move(r2);
    LSX_EXPECT_EQ(*r3.value(), 6);
}

LSX_TEST(Result, StructPayloadRoundtrip) {
    struct Rec { int id; std::string name; };
    auto r = Result::Result<Rec>(Result::Ok(Rec{1, "lsx"}));
    LSX_EXPECT_EQ(r.value().id, 1);
    LSX_EXPECT_EQ(r.value().name, std::string("lsx"));
}

LSX_TEST(Result, ChainedMapPattern) {
    // 解析 → 转换 → 使用的典型链
    auto parse = [](const std::string& s) -> Result::Result<int> {
        if (s == "42") return Result::Ok(42);
        return Result::Err(std::string("bad input: ") + s);
    };
    auto doubled = parse("42").map([](int v) { return v * 2; });
    LSX_EXPECT_EQ(doubled.value(), 84);

    auto failed = parse("x").map([](int v) { return v * 2; });
    LSX_EXPECT_TRUE(failed.isErr());
}
