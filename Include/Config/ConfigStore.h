/**
 * @file ConfigStore.h
 * @brief LIBLSX 工具库 - 通用可读写配置模块
 * @details 提供 LSX_LIB::Config 命名空间下的 ConfigStore：
 *  - INI 文件的读写（load / save），支持注释与节
 *  - 类型安全的 get<T> / set<T>（int / int64 / double / bool / string）
 *  - **全局变量绑定**：bind(section, key, &globalVar)，load 或 set 时自动同步到变量
 *  - 变更回调：onChange(...)，配置改动时通知（用于运行时重载）
 *  - 线程安全（std::shared_mutex 读写锁）
 *
 * 与既有 INIReader 的关系：INIReader 保持只读解析能力不变；ConfigStore 是
 * 面向"程序配置 + 全局变量"场景的新接口，可独立使用也可从 INIReader 迁移。
 *
 * @author 连思鑫（jinbilianshao）
 * @date 2026-11-11
 * @version 1.0
 *
 * ### 使用示例
 * @code
 * #include "Config/ConfigStore.h"
 *
 * int g_port = 8080;
 * bool g_debug = false;
 *
 * LSX_LIB::Config::ConfigStore cfg;
 * cfg.bind("server", "port", &g_port);       // 绑定到全局变量
 * cfg.bind("debug", "enabled", &g_debug);
 * cfg.load("/etc/app.ini");                  // load 后 g_port/g_debug 自动更新
 *
 * cfg.set("server", "port", 9090);           // set 也会同步到绑定变量
 * cfg.save("/etc/app.ini");
 * @endcode
 */
#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <type_traits>
#include <vector>

#include "Result/Result.h"

namespace LSX_LIB {
namespace Config {

using OpResult = Result::Result<void, std::string>;

/// 单个配置项的变更通知信息
struct ChangeEvent
{
    std::string section;
    std::string key;
    std::string old_value;
    std::string new_value;
};

/**
 * @class ConfigStore
 * @brief 可读写配置仓库（INI 持久化 + 全局变量绑定 + 变更通知）
 */
class ConfigStore
{
public:
    ConfigStore() = default;
    ~ConfigStore() = default;

    ConfigStore(const ConfigStore&) = delete;
    ConfigStore& operator=(const ConfigStore&) = delete;

    // ---- 读写文件 ----

    /// @brief 从 INI 文件加载；返回 Result（文件不存在/解析出错时携带说明）
    OpResult load(const std::string& path);

    /// @brief 保存到 INI 文件（覆盖写）；失败返回 Err
    OpResult save(const std::string& path) const;

    /// @brief 从字符串解析（便于测试或网络下发配置）
    OpResult loadFromString(const std::string& content);

    /// @brief 序列化为 INI 文本（不落盘）
    std::string toString() const;

    // ---- 类型安全读写 ----

    /// 取字符串值；不存在返回 default_value
    std::string getString(const std::string& section, const std::string& key,
                          const std::string& default_value = {}) const;

    /// 取整数；不存在或非法返回 default_value
    std::int64_t getInt(const std::string& section, const std::string& key,
                        std::int64_t default_value = 0) const;

    /// 取浮点；不存在或非法返回 default_value
    double getDouble(const std::string& section, const std::string& key,
                     double default_value = 0.0) const;

    /// 取布尔（true/yes/on/1 与 false/no/off/0，大小写不敏感）；不存在/非法返回默认值
    bool getBool(const std::string& section, const std::string& key,
                 bool default_value = false) const;

    /// 模板取值：支持 std::string / 整型 / 浮点 / bool
    template <typename T>
    T get(const std::string& section, const std::string& key, T default_value = {}) const
    {
        if constexpr (std::is_same_v<T, std::string>)
            return getString(section, key, std::move(default_value));
        else if constexpr (std::is_same_v<T, bool>)
            return getBool(section, key, default_value);
        else if constexpr (std::is_floating_point_v<T>)
            return static_cast<T>(getDouble(section, key, static_cast<double>(default_value)));
        else if constexpr (std::is_integral_v<T>)
            return static_cast<T>(getInt(section, key, static_cast<std::int64_t>(default_value)));
        else
            static_assert(sizeof(T) == 0, "ConfigStore::get<T> unsupported type");
    }

    /// 设置值（触发绑定变量同步与变更回调）
    void set(const std::string& section, const std::string& key,
             const std::string& value);
    void set(const std::string& section, const std::string& key, std::int64_t value);
    void set(const std::string& section, const std::string& key, double value);
    void set(const std::string& section, const std::string& key, bool value);

    /// 模板设置：支持 std::string / 整型 / 浮点 / bool
    template <typename T>
    void set(const std::string& section, const std::string& key, T value)
    {
        if constexpr (std::is_same_v<T, bool>) set(section, key, static_cast<bool>(value));
        else if constexpr (std::is_floating_point_v<T>) set(section, key, static_cast<double>(value));
        else if constexpr (std::is_integral_v<T>) set(section, key, static_cast<std::int64_t>(value));
        else if constexpr (std::is_convertible_v<T, std::string>) set(section, key, std::string(value));
        else static_assert(sizeof(T) == 0, "ConfigStore::set<T> unsupported type");
    }

    // ---- 查询 ----

    bool has(const std::string& section, const std::string& key) const;
    bool hasSection(const std::string& section) const;
    /// 删除一个键（触发变更回调）
    bool remove(const std::string& section, const std::string& key);
    void clear();
    std::size_t size() const;
    /// 全部节名（用于遍历）
    std::vector<std::string> sections() const;
    /// 某节下的全部键
    std::vector<std::string> keys(const std::string& section) const;

    // ---- 全局变量绑定 ----

    /**
     * @brief 把配置项绑定到全局/外部变量
     * @details load 或 set 该键时，自动把新值写入 *ptr；
     *          绑定即刻也会把当前值（或默认值）灌入变量，保证初值一致。
     * @note 变量生命周期必须长于 ConfigStore；解绑前不要销毁变量。
     */
    template <typename T>
    void bind(const std::string& section, const std::string& key, T* ptr,
              T default_value = {})
    {
        static_assert(std::is_same_v<T, std::string> || std::is_integral_v<T> ||
                      std::is_floating_point_v<T>,
                      "ConfigStore::bind<T> unsupported type");
        if (!ptr) return;
        std::unique_lock<std::shared_mutex> lk(mtx_);
        // 若配置项尚不存在，先写入默认值，保证绑定后变量有确定初值
        const auto k = makeKey(section, key);
        if (values_.find(k) == values_.end())
            values_[k] = valueToStringT<T>(default_value);
        *ptr = parseValue<T>(values_[k], default_value);
        bindings_[k] = [ptr](const std::string& v) { *ptr = parseValue<T>(v, T{}); };
        ensureSection(section);
    }

    /// 解绑某个键（不影响已保存的值）
    void unbind(const std::string& section, const std::string& key);

    /// 解绑全部
    void unbindAll();

    // ---- 变更通知 ----

    using ChangeCallback = std::function<void(const ChangeEvent&)>;

    /// 注册变更回调，返回可用于取消的句柄 id（0 表示注册失败）
    std::size_t onChange(ChangeCallback cb);
    void removeOnChange(std::size_t id);

    // ---- 与 INIReader 互操作 ----

    /// 用 INIReader 风格的只读查询（语义同 getString，便于迁移旧代码）
    std::string Get(const std::string& section, const std::string& name,
                    const std::string& default_value) const
    {
        return getString(section, name, default_value);
    }

private:
    static std::string makeKey(const std::string& section, const std::string& key);
    static std::string trim(const std::string& s);
    static std::string valueToString(const std::string& v) { return v; }
    static std::string valueToString(std::int64_t v);
    static std::string valueToString(double v);
    static std::string valueToString(bool v) { return v ? "true" : "false"; }

    /// 模板转发：消除 bind<T> 中 int/long 等整型默认值的重载歧义
    template <typename T>
    static std::string valueToStringT(T v)
    {
        if constexpr (std::is_same_v<T, std::string>) return v;
        else if constexpr (std::is_same_v<T, bool>) return valueToString(static_cast<bool>(v));
        else if constexpr (std::is_floating_point_v<T>) return valueToString(static_cast<double>(v));
        else return valueToString(static_cast<std::int64_t>(v));
    }

    template <typename T>
    static T parseValue(const std::string& s, T def)
    {
        if constexpr (std::is_same_v<T, std::string>) return s;
        else if constexpr (std::is_same_v<T, bool>)
        {
            auto t = trim(s);
            for (auto& c : t) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (t == "true" || t == "yes" || t == "on" || t == "1") return true;
            if (t == "false" || t == "no" || t == "off" || t == "0") return false;
            return def;
        }
        else if constexpr (std::is_floating_point_v<T>)
        {
            try { return static_cast<T>(std::stod(s)); } catch (...) { return def; }
        }
        else if constexpr (std::is_integral_v<T>)
        {
            try { return static_cast<T>(std::stoll(s, nullptr, 0)); } catch (...) { return def; }
        }
    }

    void ensureSection(const std::string& section);
    void applyBindingsAndNotify(const std::string& key, const std::string& old_value,
                                const std::string& new_value);

    mutable std::shared_mutex mtx_;
    std::map<std::string, std::string> values_;       // "section.key" -> 字符串值
    std::map<std::string, std::string> section_of_key_; // key -> section（序列化保序用）
    std::vector<std::string> section_order_;            // 节的原始顺序
    std::map<std::string, std::function<void(const std::string&)>> bindings_;
    std::map<std::size_t, ChangeCallback> callbacks_;
    std::size_t next_cb_id_ = 1;
};

} // namespace Config
} // namespace LSX_LIB
