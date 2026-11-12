/**
 * @file ConfigStore.cpp
 * @brief ConfigStore 实现
 */
#include "Config/ConfigStore.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace LSX_LIB {
namespace Config {

namespace
{
std::string lowerCopy(std::string s)
{
    for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}
} // namespace

std::string ConfigStore::trim(const std::string& s)
{
    std::size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

std::string ConfigStore::makeKey(const std::string& section, const std::string& key)
{
    return lowerCopy(section) + "." + lowerCopy(key);
}

std::string ConfigStore::valueToString(std::int64_t v)
{
    return std::to_string(v);
}

std::string ConfigStore::valueToString(double v)
{
    std::ostringstream oss;
    oss << std::setprecision(15) << v;
    return oss.str();
}

void ConfigStore::ensureSection(const std::string& section)
{
    if (std::find(section_order_.begin(), section_order_.end(), section) == section_order_.end())
        section_order_.push_back(section);
}

// ---------- 解析/序列化 ----------

OpResult ConfigStore::loadFromString(const std::string& content)
{
    // 先解析到临时结构：任何一行出错都直接返回，旧配置完全不受影响（原子加载）。
    std::map<std::string, std::string> parsed_values;
    std::map<std::string, std::string> parsed_section_of_key;
    std::vector<std::string> new_sections;
    std::istringstream in(content);
    std::string line, current_section;
    int lineno = 0;
    while (std::getline(in, line))
    {
        ++lineno;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::string t = trim(line);
        if (t.empty() || t[0] == ';' || t[0] == '#') continue;

        if (t.front() == '[' && t.back() == ']')
        {
            current_section = trim(t.substr(1, t.size() - 2));
            if (current_section.empty())
                return Result::Err("空节名，第 " + std::to_string(lineno) + " 行");
            if (std::find(new_sections.begin(), new_sections.end(), current_section) == new_sections.end())
                new_sections.push_back(current_section);
            continue;
        }

        std::size_t eq = t.find('=');
        if (eq == std::string::npos)
            return Result::Err("无效配置行(缺少 =)，第 " + std::to_string(lineno) + " 行");
        std::string key = trim(t.substr(0, eq));
        std::string val = trim(t.substr(eq + 1));
        if (key.empty()) return Result::Err("空键名，第 " + std::to_string(lineno) + " 行");
        if (current_section.empty())
            return Result::Err("键出现在任何节之前，第 " + std::to_string(lineno) + " 行");

        // 去掉行尾注释（仅当 ; 或 # 前有空白时才视作注释，避免误伤值中的字符）
        std::size_t comment = std::string::npos;
        for (std::size_t i = 0; i < val.size(); ++i)
        {
            if ((val[i] == ';' || val[i] == '#') && i > 0 &&
                std::isspace(static_cast<unsigned char>(val[i - 1])))
            {
                comment = i;
                break;
            }
        }
        if (comment != std::string::npos) val = trim(val.substr(0, comment));

        const auto k = makeKey(current_section, key);
        parsed_values[k] = val;
        parsed_section_of_key[k] = current_section;
    }

    // 锁内一次性提交；同时收集需要同步的绑定（回调绝不在锁内执行）。
    std::vector<std::pair<std::function<void(const std::string&)>, std::string>> pending_bindings;
    {
        std::unique_lock<std::shared_mutex> lk(mtx_);
        for (const auto& sec : new_sections) ensureSection(sec);
        for (const auto& [k, v] : parsed_values)
        {
            values_[k] = v;
            section_of_key_[k] = parsed_section_of_key.at(k);
        }
        for (const auto& [k, fn] : bindings_)
        {
            auto it = values_.find(k);
            if (it != values_.end()) pending_bindings.emplace_back(fn, it->second);
        }
    }   // mtx_ 在此释放

    // 绑定变量同步在锁外执行（写外部全局指针，无重入风险）。
    for (auto& [fn, v] : pending_bindings) if (fn) fn(v);
    return OpResult();
}

OpResult ConfigStore::load(const std::string& path)
{
    std::ifstream f(path);
    if (!f.is_open())
        return Result::Err("无法打开配置文件: " + path);
    std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    auto r = loadFromString(content);
    if (r.isErr()) return Result::Err(path + ": " + r.error());
    return OpResult();
}

std::string ConfigStore::toString() const
{
    std::shared_lock<std::shared_mutex> lk(mtx_);
    std::ostringstream out;
    // 按节分组，保持节的原始顺序
    std::map<std::string, std::vector<std::pair<std::string, std::string>>> by_section;
    for (const auto& [k, v] : values_)
    {
        std::string sec = section_of_key_.count(k) ? section_of_key_.at(k) : std::string{};
        std::size_t dot = k.find('.');
        std::string key = (dot == std::string::npos) ? k : k.substr(dot + 1);
        by_section[sec].push_back({key, v});
    }
    std::vector<std::string> order = section_order_;
    for (const auto& [sec, dummy] : by_section)
    {
        (void)dummy;
        if (std::find(order.begin(), order.end(), sec) == order.end()) order.push_back(sec);
    }

    for (const auto& sec : order)
    {
        auto it = by_section.find(sec);
        if (it == by_section.end() || it->second.empty()) continue;
        out << "[" << sec << "]" << std::endl;
        for (const auto& [k, v] : it->second) out << k << " = " << v << std::endl;
        out << std::endl;
    }
    return out.str();
}

OpResult ConfigStore::save(const std::string& path) const
{
    std::string text = toString();
    std::ofstream f(path, std::ios::trunc);
    if (!f.is_open()) return Result::Err("无法写入配置文件: " + path);
    f << text;
    if (!f.good()) return Result::Err("写入配置文件失败: " + path);
    return OpResult();
}

// ---------- 类型安全读取 ----------

std::string ConfigStore::getString(const std::string& section, const std::string& key,
                                   const std::string& default_value) const
{
    std::shared_lock<std::shared_mutex> lk(mtx_);
    auto it = values_.find(makeKey(section, key));
    return it == values_.end() ? default_value : it->second;
}

std::int64_t ConfigStore::getInt(const std::string& section, const std::string& key,
                                 std::int64_t default_value) const
{
    std::string v = getString(section, key, {});
    if (v.empty()) return default_value;
    try { return std::stoll(v, nullptr, 0); } catch (...) { return default_value; }
}

double ConfigStore::getDouble(const std::string& section, const std::string& key,
                              double default_value) const
{
    std::string v = getString(section, key, {});
    if (v.empty()) return default_value;
    try { return std::stod(v); } catch (...) { return default_value; }
}

bool ConfigStore::getBool(const std::string& section, const std::string& key,
                          bool default_value) const
{
    std::string v = getString(section, key, {});
    if (v.empty()) return default_value;
    std::string t = lowerCopy(trim(v));
    if (t == "true" || t == "yes" || t == "on" || t == "1") return true;
    if (t == "false" || t == "no" || t == "off" || t == "0") return false;
    return default_value;
}

// ---------- 写入 ----------

void ConfigStore::set(const std::string& section, const std::string& key,
                      const std::string& value)
{
    std::function<void(const std::string&)> binding;
    std::vector<ChangeCallback> cbs;
    std::string old;
    bool changed = false;
    {
        // 锁内只改数据并收集待通知项；绑定同步与用户回调在锁外执行——
        // 回调里调用 set/has/remove 等公开方法不会再造成 shared_mutex 重入死锁
        std::unique_lock<std::shared_mutex> lk(mtx_);
        const auto k = makeKey(section, key);
        old = values_.count(k) ? values_[k] : std::string{};
        values_[k] = value;
        section_of_key_[k] = section;
        ensureSection(section);
        auto bit = bindings_.find(k);
        if (bit != bindings_.end()) binding = bit->second;
        changed = (old != value);
        if (changed)
        {
            ChangeEvent ev{section, key, old, value};
            for (auto& entry : callbacks_) if (entry.second) cbs.push_back(entry.second);
        }
    }
    if (binding) binding(value);            // 绑定变量同步（写外部全局变量，无锁）
    if (changed)
    {
        ChangeEvent ev{section, key, old, value};
        for (auto& cb : cbs) if (cb) cb(ev); // 用户回调锁外执行
    }
}

void ConfigStore::set(const std::string& section, const std::string& key, std::int64_t value)
{
    set(section, key, valueToString(value));
}
void ConfigStore::set(const std::string& section, const std::string& key, double value)
{
    set(section, key, valueToString(value));
}
void ConfigStore::set(const std::string& section, const std::string& key, bool value)
{
    set(section, key, valueToString(value));
}

void ConfigStore::applyBindingsAndNotify(const std::string& key, const std::string& old_value,
                                         const std::string& new_value)
{
    (void)key; (void)old_value; (void)new_value;
}

// ---------- 查询 ----------

bool ConfigStore::has(const std::string& section, const std::string& key) const
{
    std::shared_lock<std::shared_mutex> lk(mtx_);
    return values_.count(makeKey(section, key)) != 0;
}

bool ConfigStore::hasSection(const std::string& section) const
{
    std::shared_lock<std::shared_mutex> lk(mtx_);
    return std::find(section_order_.begin(), section_order_.end(), section) != section_order_.end();
}

bool ConfigStore::remove(const std::string& section, const std::string& key)
{
    std::vector<ChangeCallback> cbs;
    std::string old;
    bool existed = false;
    {
        std::unique_lock<std::shared_mutex> lk(mtx_);
        const auto k = makeKey(section, key);
        auto it = values_.find(k);
        if (it == values_.end()) return false;
        old = it->second;
        values_.erase(it);
        existed = true;
        ChangeEvent ev{section, key, old, {}};
        for (auto& entry : callbacks_) if (entry.second) cbs.push_back(entry.second);
    }
    if (existed)
    {
        ChangeEvent ev{section, key, old, {}};
        for (auto& cb : cbs) if (cb) cb(ev);
    }
    return existed;
}

void ConfigStore::clear()
{
    std::unique_lock<std::shared_mutex> lk(mtx_);
    values_.clear();
    section_of_key_.clear();
    section_order_.clear();
    // 绑定保留：clear 后变量仍停留在上次同步值，重新 load/set 会再次同步
}

std::size_t ConfigStore::size() const
{
    std::shared_lock<std::shared_mutex> lk(mtx_);
    return values_.size();
}

std::vector<std::string> ConfigStore::sections() const
{
    std::shared_lock<std::shared_mutex> lk(mtx_);
    return section_order_;
}

std::vector<std::string> ConfigStore::keys(const std::string& section) const
{
    std::shared_lock<std::shared_mutex> lk(mtx_);
    std::vector<std::string> out;
    std::string prefix = lowerCopy(section) + ".";
    for (const auto& entry : values_)
        if (entry.first.compare(0, prefix.size(), prefix) == 0)
            out.push_back(entry.first.substr(prefix.size()));
    return out;
}

// ---------- 绑定 ----------

void ConfigStore::unbind(const std::string& section, const std::string& key)
{
    std::unique_lock<std::shared_mutex> lk(mtx_);
    bindings_.erase(makeKey(section, key));
}

void ConfigStore::unbindAll()
{
    std::unique_lock<std::shared_mutex> lk(mtx_);
    bindings_.clear();
}

// ---------- 变更通知 ----------

std::size_t ConfigStore::onChange(ChangeCallback cb)
{
    if (!cb) return 0;
    std::unique_lock<std::shared_mutex> lk(mtx_);
    std::size_t id = next_cb_id_++;
    callbacks_[id] = std::move(cb);
    return id;
}

void ConfigStore::removeOnChange(std::size_t id)
{
    std::unique_lock<std::shared_mutex> lk(mtx_);
    callbacks_.erase(id);
}

} // namespace Config
} // namespace LSX_LIB
