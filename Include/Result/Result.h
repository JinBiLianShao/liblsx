/**
 * @file Result.h
 * @brief LIBLSX 工具库 - Result 统一错误处理类型
 * @details 提供 LSX_LIB::Result 命名空间下的 Result<T, E>：
 *          值或错误的二者取一（C++17 没有 std::expected 的过渡方案）。
 *
 * 设计目标：统一库内"bool + 输出参数"/"魔法返回值"等不一致的错误风格。
 *
 * @author 连思鑫（jinbilianshao）
 * @date 2026-11-10
 * @version 1.0
 *
 * ### 使用示例
 * @code
 * LSX_LIB::Result::Result<int, std::string> parse(const char* s);
 *
 * auto r = parse("42");
 * if (r.isOk()) {
 *     use(r.value());          // 42
 * } else {
 *     log(r.error());          // 错误信息
 * }
 *
 * // void 特化：只关心成败
 * LSX_LIB::Result::Result<void> save();
 * if (save().isErr()) { handle(); }
 *
 * // valueOr：失败给默认值
 * int port = parse_port(cfg).valueOr(8080);
 * @endcode
 
 *
 * ### 线程安全 (@thread_safety)
 * Result 值语义，多线程各自持有各自的实例即可；无共享状态。
 */
#pragma once

#include <cassert>
#include <string>
#include <utility>
#include <variant>

namespace LSX_LIB {
namespace Result {

/// 构造成功分支的标签类型
template <typename T>
struct OkTag
{
    T value;
    explicit OkTag(T v) : value(std::move(v)) {}
};

/// 构造失败分支的标签类型
template <typename E>
struct ErrTag
{
    E error;
    explicit ErrTag(E e) : error(std::move(e)) {}
};

/// Ok(value) 辅助函数（推导 T）
template <typename T>
OkTag<T> Ok(T v)
{
    return OkTag<T>(std::move(v));
}

/// Err(error) 辅助函数（推导 E）
template <typename E>
ErrTag<E> Err(E e)
{
    return ErrTag<E>(std::move(e));
}

/**
 * @class Result
 * @brief 值或错误的二者取一
 * @tparam T 成功值类型
 * @tparam E 错误类型（默认 std::string）
 */
template <typename T, typename E = std::string>
class Result
{
public:
    Result(OkTag<T> ok) : storage_(std::move(ok.value)), has_value_(true) {}
    Result(ErrTag<E> err) : storage_(std::move(err.error)), has_value_(false) {}

    Result(const Result&) = default;
    Result& operator=(const Result&) = default;
    Result(Result&&) = default;
    Result& operator=(Result&&) = default;
    ~Result() = default;

    bool isOk() const { return has_value_; }
    bool isErr() const { return !has_value_; }

    /// @brief 访问成功值（Err 状态访问视为编程错误，assert fail fast）
    const T& value() const
    {
        assert(has_value_ && "Result::value() called on Err state");
        return std::get<T>(storage_);
    }

    T& value()
    {
        assert(has_value_ && "Result::value() called on Err state");
        return std::get<T>(storage_);
    }

    /// @brief 访问错误（Ok 状态访问视为编程错误，assert fail fast）
    const E& error() const
    {
        assert(!has_value_ && "Result::error() called on Ok state");
        return std::get<E>(storage_);
    }

    /// @brief 取值；Err 时返回 fallback（不改变状态，不 assert）
    T valueOr(T fallback) const
    {
        return has_value_ ? std::get<T>(storage_) : std::move(fallback);
    }

    /// @brief 单调操作：Ok 时以 fn 映射值，Err 时原样传递错误
    template <typename Fn>
    auto map(Fn fn) const -> Result<decltype(fn(std::declval<const T&>())), E>
    {
        using U = decltype(fn(std::declval<const T&>()));
        if (has_value_)
        {
            return Result<U, E>(OkTag<U>(fn(std::get<T>(storage_))));
        }
        return Result<U, E>(ErrTag<E>(std::get<E>(storage_)));
    }

private:
    std::variant<T, E> storage_;
    bool has_value_;
};

/**
 * @brief Result<void, E> 特化：只关心成败与错误信息
 */
template <typename E>
class Result<void, E>
{
public:
    Result() : has_value_(true) {}
    Result(ErrTag<E> err) : storage_(std::move(err.error)), has_value_(false) {}

    bool isOk() const { return has_value_; }
    bool isErr() const { return !has_value_; }

    const E& error() const
    {
        assert(!has_value_ && "Result::error() called on Ok state");
        return storage_;
    }

private:
    E storage_;
    bool has_value_;
};

/// Result<void> 便利别名（默认 string 错误）
using VoidResult = Result<void, std::string>;

/// Result<void> 的 Ok 构造辅助
inline Result<void, std::string> OkVoid() { return Result<void, std::string>(); }

} // namespace Result
} // namespace LSX_LIB
