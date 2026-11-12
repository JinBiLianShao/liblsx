/**
 * @file example_config.cpp
 * @brief LIBLSX Config（ConfigStore）模块使用示例
 *
 * 演示内容：
 *   1. INI 字符串解析 / 序列化（loadFromString / toString）
 *   2. 类型安全 get / set（int/double/bool/string）
 *   3. 全局变量绑定 bind（load 或 set 自动同步到变量）
 *   4. 变更回调 onChange（配置改动时通知）
 *
 * 关键点：
 *   - load 失败（文件不存在/解析出错）返回 Err 且不污染旧配置（原子加载）。
 *   - 键不区分大小写；注释用 ; 或 # 开头。
 *   - 绑定变量的生命周期必须长于 ConfigStore。
 */

#include "Config/ConfigStore.h"

#include <iostream>
#include <string>

namespace cf = LSX_LIB::Config;

int main()
{
    std::cout << "===== ConfigStore 配置模块示例 =====\n";

    cf::ConfigStore cfg;

    // ---- 1) 从 INI 字符串加载 ----
    const char* ini =
        "[server]\n"
        "port = 8080\n"
        "host = 127.0.0.1\n"
        "debug = true\n"
        "\n"
        "[sensor]\n"
        "threshold = 3.75\n";
    if (auto r = cfg.loadFromString(ini); r.isErr()) {
        std::cerr << "  加载失败: " << r.error() << "\n";
        return 1;
    }

    // ---- 2) 类型安全读取 ----
    std::cout << "  port      = " << cfg.getInt("server", "port") << "\n";
    std::cout << "  host      = " << cfg.getString("server", "host") << "\n";
    std::cout << "  debug     = " << (cfg.getBool("server", "debug") ? "true" : "false") << "\n";
    std::cout << "  threshold = " << cfg.getDouble("sensor", "threshold") << "\n";

    // ---- 3) 全局变量绑定 ----
    int g_port = 0;
    cfg.bind("server", "port", &g_port);   // 绑定：立即同步当前值 8080
    std::cout << "  绑定后 g_port = " << g_port << "\n";

    cfg.set("server", "port", 9090);       // set 会同步到绑定变量
    std::cout << "  set(9090) 后 g_port = " << g_port << "\n";

    // ---- 4) 变更回调 ----
    auto id = cfg.onChange([](const cf::ChangeEvent& e) {
        std::cout << "  [onChange] " << e.section << "." << e.key
                  << ": " << e.old_value << " -> " << e.new_value << "\n";
    });
    cfg.set("server", "host", std::string("10.0.0.1"));   // 触发回调
    cfg.removeOnChange(id);

    // ---- 5) 序列化回 INI 文本 ----
    std::cout << "  ---- toString() ----\n" << cfg.toString();

    // ---- 6) 文件读写 ----
    if (auto r = cfg.save("/tmp/lsx_config_example.ini"); r.isOk()) {
        std::cout << "  已保存到 /tmp/lsx_config_example.ini\n";
    }

    return 0;
}
