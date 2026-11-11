# LIBLSX 工具库 ConfigStore 模块使用说明（通用读写配置）

## 模块概述

ConfigStore 提供**可读写的通用配置管理**，专为「程序里有一堆全局变量要用配置文件配」的场景设计。命名空间：LSX_LIB::Config。

与库里既有的 INIReader 区别：

| | INIReader（既有） | ConfigStore（v0.6.0 新增） |
|---|---|---|
| 读 | 支持 | 支持 |
| 写/保存 | 不支持 | set + save |
| 全局变量绑定 | 无 | bind(section, key, &globalVar) |
| 变更通知 | 无 | onChange 回调 |
| 错误风格 | ParseError 行号 | Result（携带错误说明） |
| 线程安全 | 无保证 | shared_mutex 读写锁 |

INIReader 保持原样不动（下游兼容）；新代码推荐用 ConfigStore。

## 快速上手：全局变量绑定（核心用法）

    #include "Config/ConfigStore.h"

    // 程序里的全局配置变量
    int         g_port  = 8080;
    bool        g_debug = false;
    std::string g_host  = "localhost";
    double      g_ratio = 1.0;

    LSX_LIB::Config::ConfigStore cfg;

    // 绑定即刻用默认值灌入变量，保证初值确定
    cfg.bind("server", "port",    &g_port,  8080);
    cfg.bind("server", "host",    &g_host,  std::string("localhost"));
    cfg.bind("debug",  "enabled", &g_debug, false);
    cfg.bind("app",    "ratio",   &g_ratio, 1.0);

    // 从文件加载：所有绑定变量自动更新
    auto r = cfg.load("/etc/myapp.ini");
    if (r.isErr()) std::cerr << r.error() << std::endl;

    // 运行时改配置：set 也会同步到绑定变量
    cfg.set("server", "port", 9090);       // g_port 立刻变成 9090
    cfg.save("/etc/myapp.ini");            // 写回文件，下次启动生效

对应 INI 文件：

    [server]
    port = 9090
    host = 10.0.0.1

    [debug]
    enabled = yes

    [app]
    ratio = 1.5

## 类型安全读写

    // 带默认值的读取（不存在或格式非法时返回默认值，绝不抛异常）
    int64_t port = cfg.getInt("server", "port", 8080);
    double  r    = cfg.getDouble("app", "ratio", 1.0);
    bool    dbg  = cfg.getBool("debug", "enabled", false);
    std::string h = cfg.getString("server", "host", "localhost");

    // 模板接口
    auto p = cfg.get<int>("server", "port");
    auto s = cfg.get<std::string>("server", "host", "default");

    // 写入（多种类型重载）
    cfg.set("server", "port", 8080);
    cfg.set("app", "ratio", 2.5);
    cfg.set("debug", "enabled", true);
    cfg.set("server", "host", std::string("1.2.3.4"));

布尔值解析：true/yes/on/1 与 false/no/off/0，大小写不敏感。整数支持十进制、0x 十六进制（0x4d2 = 1234）与负数；浮点支持科学计数法。

## 变更回调（运行时重载）

    auto id = cfg.onChange([](const LSX_LIB::Config::ChangeEvent& e) {
        std::cout << e.section << "." << e.key << ": "
                  << e.old_value << " -> " << e.new_value << std::endl;
    });
    cfg.removeOnChange(id);   // 取消

只在值真正变化时触发（重复 set 同值不通知）；remove() 删除键也会触发。常与 EventBus 组合：配置变更时广播事件给各模块。

## 其它接口

| 方法 | 说明 |
|------|------|
| load(path) / save(path) | INI 文件读写，返回 Result<void> |
| loadFromString(text) / toString() | 内存解析/序列化 |
| has(section, key) / hasSection(section) | 存在性查询 |
| sections() / keys(section) | 遍历 |
| remove(section, key) / clear() | 删除 |
| size() | 配置项数量 |
| unbind(section, key) / unbindAll() | 解除变量绑定 |
| Get(section, name, default) | INIReader 风格只读别名（便于迁移旧代码） |

## 文件格式约定

- 节：[section]，键：key = value
- 注释：行首 ; 或 #；行尾注释要求分隔符前有空白（port = 8080 ; 备注）
- 节名与键名**大小写不敏感**（内部统一小写），保存时按首次出现的节顺序输出
- 解析错误（无节、空节名、缺 =）返回 Err 并带行号说明

## 线程安全

std::shared_mutex 保护：多个读可并发，写独占。**绑定变量的读写不受锁保护**（是外部全局变量），多线程同时 set 与读变量时请自行同步，或只在初始化/重载阶段做绑定更新。

## 常见错误用法与规避

1. **绑定变量生命周期**：bind 存的是指针，变量必须活得比 ConfigStore 久；局部变量不要绑。
2. **忘记检查 load 的 Result**：Err 时配置未加载，绑定变量保持默认值——通常可安全降级，但要记录日志。
3. **注释里的分号在值里**：name = a;b 不会被截断（无前置空白），符合预期；若要截断请写 a ;comment。
4. **与 INIReader 混用**：两者解析结果不共享；选一个用，或用 Get() 别名过渡。
