/**
 * @file INIReader.h
 * @brief INI 配置文件解析器
 * @details 提供 INI 文件的读取和解析功能，支持多种数据类型获取和灵活的配置管理。INI 文件格式通常用于存储程序的配置信息，易于阅读和编辑。
 * @author 连思鑫
 * @date 2025-2-10
 * @version 1.0
 *
 * ### 核心功能
 * - **读取 INI 文件**: 可以从指定的文件路径或文件流中读取 INI 格式的配置文件。
 * - **解析 INI 文件**: 将 INI 文件内容解析为易于访问的键值对，支持节（section）和名称（name）/值（value）对。
 * - **支持多种数据类型**: 提供便捷的接口获取字符串、整数、浮点数和布尔值等多种数据类型配置项。
 * - **灵活的配置管理**: 允许通过节名和名称快速检索配置值，并提供默认值，方便处理配置项缺失的情况。
 *
 * ### 使用示例
 *
 * 示例代码 `main.cpp` 演示了如何使用 `INIReader` 类来读取和解析 INI 配置文件，并获取配置项的值。
 * 配置文件 `config.ini` 和 示例代码 `main.cpp` 内容如下：
 *
 * @code
 * main.cpp:
 * ```c++
 * #include "INIReader.h"
 * #include <iostream>
 * #include <string>
 *
 * int main() {
 *     INIReader config("config.ini"); // 加载配置文件
 *
 *     if (config.ParseError() < 0) { // 检查解析错误
 *         std::cerr << "Error loading 'config.ini'" << std::endl;
 *         return 1;
 *     }
 *
 *     std::cout << "Config loaded from 'config.ini':" << std::endl;
 *
 *     // 获取 SERVER 节的配置
 *     std::cout << "\n[SERVER] section:" << std::endl;
 *     std::cout << "  IP Address  = " << config.Get("SERVER", "ip", "0.0.0.0") << std::endl; // 获取字符串，提供默认值 "0.0.0.0"
 *     std::cout << "  Port        = " << config.GetInteger("SERVER", "port", 80) << std::endl; // 获取整数，提供默认值 80
 *     std::cout << "  Timeout     = " << config.GetInteger("SERVER", "timeout", 60) << std::endl; // 获取整数，提供默认值 60 (秒)
 *
 *
 *     // 获取 DATABASE 节的配置
 *     std::cout << "\n[DATABASE] section:" << std::endl;
 *     std::cout << "  Type        = " << config.Get("DATABASE", "type", "unknown") << std::endl; // 获取字符串，默认值 "unknown"
 *     std::cout << "  Host        = " << config.Get("DATABASE", "host", "localhost") << std::endl;
 *     std::cout << "  DB Name     = " << config.Get("DATABASE", "dbname", "") << std::endl; // 获取字符串，默认值为空字符串 ""
 *     std::cout << "  User        = " << config.Get("DATABASE", "user", "") << std::endl;
 *     std::cout << "  Password    = " << config.Get("DATABASE", "password", "") << std::endl;
 *
 *     // 获取 DEBUG 节的配置
 *     std::cout << "\n[DEBUG] section:" << std::endl;
 *     std::cout << "  Enabled     = " << config.GetBoolean("DEBUG", "enabled", false) << std::endl; // 获取布尔值，默认值 false
 *     std::cout << "  Log Level   = " << config.Get("DEBUG", "log_level", "DEBUG") << std::endl; // 获取字符串，默认值 "DEBUG"
 *
 *     // 获取 APPLICATION 节的配置
 *     std::cout << "\n[APPLICATION] section:" << std::endl;
 *     std::cout << "  Name        = " << config.Get("APPLICATION", "name", "DefaultApp") << std::endl; // 获取字符串，默认值 "DefaultApp"
 *     std::cout << "  Version     = " << config.Get("APPLICATION", "version", "1.0.0") << std::endl; // 获取字符串，默认值 "1.0.0"
 *     std::cout << "  Description = " << config.Get("APPLICATION", "description", "No description") << std::endl; // 获取字符串，默认值 "No description"
 *     std::cout << "  Authors     = " << config.Get("APPLICATION", "authors", "Unknown") << std::endl; // 获取字符串，默认值 "Unknown"
 *     std::cout << "  Multi-line  = " << std::endl << config.Get("APPLICATION", "multi_line_value", "") << std::endl; // 获取多行字符串
 *
 *     // 获取不存在的配置项，测试默认值
 *     std::cout << "\n[NON_EXISTENT_SECTION] section:" << std::endl;
 *     std::cout << "  Value       = " << config.Get("NON_EXISTENT_SECTION", "non_existent_key", "Default Value") << std::endl; // 测试默认值
 *
 *     // 打印所有节名
 *     std::cout << "\nAll sections found in INI file:" << std::endl;
 *     const std::set<std::string>& sections = config.Sections();
 *     for (const std::string& section_name : sections) {
 *         std::cout << "  " << section_name << std::endl;
 *     }
 *
 *
 *     return 0;
 * }
 * ```
 *
 * `config.ini`:
 * ```ini
 * ; 这是一个示例 INI 配置文件
 * ; 用于演示 INIReader 的功能
 *
 * [SERVER]
 * ip = 192.168.1.100  ; 服务器 IP 地址
 * port = 8080         # 服务器端口号
 * timeout = 30        ; 超时时间，单位秒
 *
 * [DATABASE]
 * type = mysql
 * host = localhost
 * dbname = example_db
 * user = db_user
 * password = secure_password
 *
 * [DEBUG]
 * enabled = true      ; 是否启用调试模式 (true/false/yes/no/off/0)
 * log_level = INFO    ; 日志级别
 *
 * [APPLICATION]
 * name = MyApp
 * version = 1.2.3
 * description = "Example application configuration"
 * authors = "连思鑫, Other Contributors" ; 多个作者
 * multi_line_value = This is a
 *                    multi-line value.
 *                    It spans across
 *                    multiple lines.
 *
 * ; 空节示例
 * [EMPTY_SECTION]
 * ```
 * @endcode
 *
 * ### 注意事项
 * - **INI 文件结构**: INI 文件由节（section）和键值对（name=value）组成。节名用方括号 `[]` 括起来，键值对在节下定义。
 * - **注释**: 支持使用 `;` 或 `#` 开头的行作为注释，注释从 `;` 或 `#` 字符开始，直到行尾。
 * - **大小写不敏感**: 节名和键名在检索时是大小写不敏感的，例如 `[SERVER]` 和 `[server]`，`ip` 和 `IP` 会被视为相同的。
 * - **多行值**: 支持多行值，后续行以空格或制表符开头，将被视为前一个值的延续（通过宏 `INI_ALLOW_MULTILINE` 控制）。多行值在获取时，行与行之间会自动合并为一个字符串，并用换行符 `\n` 分隔。
 * - **错误处理**: 解析错误会返回错误行号，文件打开错误返回 -1。可以通过 `ParseError()` 方法检查解析过程中是否发生错误。
 */

#ifndef __INI_H__
#define __INI_H__

/* 为了更容易在 C++ 代码中包含此头文件 */
#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

/**
 * @brief 处理函数原型typedef。
 *
 * 此 typedef 定义了处理函数 `ini_handler` 的原型，
 * 该函数在解析 INI 文件时，对于每个解析到的 name=value 对都会被调用。
 *
 * @param user 用户提供的指针，在调用 `ini_parse` 函数时传入。
 * @param section 当前解析到的节名。
 * @param name 当前解析到的名称。
 * @param value 当前解析到的值。
 * @return 成功时返回非零值，错误时返回零。
 */
typedef int (*ini_handler)(void *user, const char *section, const char *name,
                           const char *value);

/**
 * @brief fgets 风格的读取函数原型typedef。
 *
 * 此 typedef 定义了读取函数 `ini_reader` 的原型，
 * 该函数模仿 `fgets` 的行为，从数据流中读取一行数据。
 *
 * @param str 存储读取到的字符串的缓冲区。
 * @param num 缓冲区的大小。
 * @param stream 数据流指针。
 * @return 成功时返回 `str`，读取失败或到达文件末尾时返回 NULL。
 */
typedef char *(*ini_reader)(char *str, int num, void *stream);

/**
 * @brief 解析给定的 INI 风格文件。
 *
 * 可以包含 `[section]`，`name=value` 对（去除空白字符），以及以 `;`（分号）开头的注释。
 * 如果在任何节标题之前解析到 `name=value` 对，则节名为 ""。
 * 也支持 `name:value` 对，以兼容 Python 的 configparser。
 *
 * 对于每个解析到的 `name=value` 对，会调用处理函数，并传入用户指针以及节名、名称和值
 * （数据仅在处理函数调用期间有效）。处理函数应在成功时返回非零值，错误时返回零。
 *
 * @param filename INI 文件名。
 * @param handler 处理函数，用于处理解析到的每个 name=value 对。
 * @param user 用户提供的指针，将传递给处理函数。
 * @return 成功时返回 0，解析错误时返回第一个错误所在的行号（不会在第一个错误时停止），
 *         文件打开错误时返回 -1，内存分配错误时返回 -2（仅当 INI_USE_STACK 为零时）。
 */
int ini_parse(const char *filename, ini_handler handler, void *user);

/**
 * @brief 与 ini_parse() 相同，但接受 FILE* 而不是文件名。
 *
 * 完成后不会关闭文件，调用者必须负责关闭文件。
 *
 * @param file 文件指针。
 * @param handler 处理函数。
 * @param user 用户提供的指针。
 * @return 成功时返回 0，解析错误时返回第一个错误所在的行号，文件打开错误时返回 -1，内存分配错误时返回 -2。
 */
int ini_parse_file(FILE *file, ini_handler handler, void *user);

/**
 * @brief 与 ini_parse() 相同，但接受 ini_reader 函数指针而不是文件名。
 *
 * 用于实现自定义或基于字符串的 I/O。
 *
 * @param reader ini_reader 函数指针。
 * @param stream 数据流指针，将传递给 reader 函数。
 * @param handler 处理函数。
 * @param user 用户提供的指针。
 * @return 成功时返回 0，解析错误时返回第一个错误所在的行号，内存分配错误时返回 -2。
 */
int ini_parse_stream(ini_reader reader, void *stream, ini_handler handler,
                     void *user);

/**
 * @brief 非零值表示允许解析多行值，风格类似于 Python 的 configparser。
 *
 * 如果允许，`ini_parse()` 将对每个后续解析到的行使用相同的名称调用处理函数。
 */
#ifndef INI_ALLOW_MULTILINE
#define INI_ALLOW_MULTILINE 1
#endif

/**
 * @brief 非零值表示允许文件开头存在 UTF-8 BOM 序列 (0xEF 0xBB 0xBF)。
 *
 * 参见 http://code.google.com/p/inih/issues/detail?id=21
 */
#ifndef INI_ALLOW_BOM
#define INI_ALLOW_BOM 1
#endif

/**
 * @brief 非零值表示允许行内注释（使用 INI_INLINE_COMMENT_PREFIXES 指定的有效行内注释前缀字符）。
 *
 * 设置为 0 可以关闭此功能，并匹配 Python 3.2+ configparser 的行为。
 */
#ifndef INI_ALLOW_INLINE_COMMENTS
#define INI_ALLOW_INLINE_COMMENTS 1
#endif
#ifndef INI_INLINE_COMMENT_PREFIXES
#define INI_INLINE_COMMENT_PREFIXES ";"
#endif

/**
 * @brief 非零值表示使用栈，零表示使用堆（malloc/free）。
 */
#ifndef INI_USE_STACK
#define INI_USE_STACK 1
#endif

/**
 * @brief 在第一个错误时停止解析（默认是继续解析）。
 */
#ifndef INI_STOP_ON_FIRST_ERROR
#define INI_STOP_ON_FIRST_ERROR 0
#endif

/**
 * @brief INI 文件中任何行的最大行长度。
 */
#ifndef INI_MAX_LINE
#define INI_MAX_LINE 200
#endif

#ifdef __cplusplus
}
#endif

#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#if !INI_USE_STACK
#include <stdlib.h>
#endif

#define MAX_SECTION 50  /**< 节名最大长度 */
#define MAX_NAME 50     /**< 键名最大长度 */

/**
 * @brief 从给定字符串末尾去除空白字符，原地修改。
 *
 * @param s 要处理的字符串。
 * @return 返回 `s`。
 */
inline static char *rstrip(char *s) {
    char *p = s + strlen(s);
    while (p > s && isspace((unsigned char) (*--p)))
        *p = '\0';
    return s;
}

/**
 * @brief 返回给定字符串中第一个非空白字符的指针。
 *
 * @param s 要处理的字符串。
 * @return 第一个非空白字符的指针。
 */
inline static char *lskip(const char *s) {
    while (*s && isspace((unsigned char) (*s)))
        s++;
    return (char *) s;
}

/**
 * @brief 返回给定字符串中第一个字符（chars 中的字符）或行内注释的指针，
 *        如果两者都未找到，则返回指向字符串末尾空字符的指针。
 *        行内注释必须以空白字符作为前缀才能注册为注释。
 *
 * @param s 要搜索的字符串。
 * @param chars 要查找的字符集合，可以为 NULL。
 * @return 第一个匹配字符或行内注释的指针，或者字符串末尾的空字符指针。
 */
inline static char *find_chars_or_comment(const char *s, const char *chars) {
#if INI_ALLOW_INLINE_COMMENTS
    int was_space = 0;
    while (*s && (!chars || !strchr(chars, *s)) &&
           !(was_space && strchr(INI_INLINE_COMMENT_PREFIXES, *s))) {
        was_space = isspace((unsigned char) (*s));
        s++;
    }
#else
    while (*s && (!chars || !strchr(chars, *s))) {
      s++;
    }
#endif
    return (char *) s;
}

/**
 * @brief strncpy 的版本，确保 dest (size 字节) 以 null 结尾。
 *
 * @param dest 目标缓冲区。
 * @param src 源字符串。
 * @param size 目标缓冲区大小。
 * @return 返回 `dest`。
 */
inline static char *strncpy0(char *dest, const char *src, size_t size) {
    strncpy(dest, src, size);
    dest[size - 1] = '\0';
    return dest;
}

/**
 * @brief 参见头文件中的文档。
 * @param reader
 * @param stream
 * @param handler
 * @param user
 * @return
 */
inline int ini_parse_stream(ini_reader reader, void *stream,
                            ini_handler handler, void *user) {
    /* 使用相当多的栈空间（如果需要，请改用堆） */
#if INI_USE_STACK
    char line[INI_MAX_LINE];
#else
    char *line;
#endif
    char section[MAX_SECTION] = "";
    char prev_name[MAX_NAME] = "";

    char *start;
    char *end;
    char *name;
    char *value;
    int lineno = 0;
    int error = 0;

#if !INI_USE_STACK
    line = (char *)malloc(INI_MAX_LINE);
    if (!line) {
      return -2;
    }
#endif

    /* 逐行扫描数据流 */
    while (reader(line, INI_MAX_LINE, stream) != NULL) {
        lineno++;

        start = line;
#if INI_ALLOW_BOM
        if (lineno == 1 && (unsigned char) start[0] == 0xEF &&
            (unsigned char) start[1] == 0xBB && (unsigned char) start[2] == 0xBF) {
            start += 3;
        }
#endif
        start = lskip(rstrip(start));

        if (*start == ';' || *start == '#') {
            /* 按照 Python configparser 的规定，允许 ; 和 # 注释行开头 */
        }
#if INI_ALLOW_MULTILINE
        else if (*prev_name && *start && start > line) {
#if INI_ALLOW_INLINE_COMMENTS
            end = find_chars_or_comment(start, NULL);
            if (*end)
                *end = '\0';
            rstrip(start);
#endif

            /* 非空白行且以空白符开头，视为先前名称值的延续 (按照 Python configparser) */
            if (!handler(user, section, prev_name, start) && !error)
                error = lineno;
        }
#endif
        else if (*start == '[') {
            /* "[section]" 行 */
            end = find_chars_or_comment(start + 1, "]");
            if (*end == ']') {
                *end = '\0';
                strncpy0(section, start + 1, sizeof(section));
                *prev_name = '\0';
            } else if (!error) {
                /* 在节行中未找到 ']' */
                error = lineno;
            }
        } else if (*start) {
            /* 不是注释，必须是 name[=:]value 对 */
            end = find_chars_or_comment(start, "=:");
            if (*end == '=' || *end == ':') {
                *end = '\0';
                name = rstrip(start);
                value = lskip(end + 1);
#if INI_ALLOW_INLINE_COMMENTS
                end = find_chars_or_comment(value, NULL);
                if (*end)
                    *end = '\0';
#endif
                rstrip(value);

                /* 找到有效的 name[=:]value 对，调用处理函数 */
                strncpy0(prev_name, name, sizeof(prev_name));
                if (!handler(user, section, name, value) && !error)
                    error = lineno;
            } else if (!error) {
                /* 在 name[=:]value 行中未找到 '=' 或 ':' */
                error = lineno;
            }
        }

#if INI_STOP_ON_FIRST_ERROR
        if (error)
          break;
#endif
    }

#if !INI_USE_STACK
    free(line);
#endif

    return error;
}

/**
 * @brief 参见头文件中的文档。
 * @param file
 * @param handler
 * @param user
 * @return
 */
inline int ini_parse_file(FILE *file, ini_handler handler, void *user) {
    return ini_parse_stream((ini_reader) fgets, file, handler, user);
}

/**
 * @brief 参见头文件中的文档。
 * @param filename
 * @param handler
 * @param user
 * @return
 */
inline int ini_parse(const char *filename, ini_handler handler, void *user) {
    FILE *file;
    int error;

    file = fopen(filename, "r");
    if (!file)
        return -1;
    error = ini_parse_file(file, handler, user);
    fclose(file);
    return error;
}

#endif /* __INI_H__ */

#ifndef __INIREADER_H__
#define __INIREADER_H__

#include <map>
#include <set>
#include <string>

namespace LSX_LIB::Config {
/**
 * @class INIReader
 * @brief INI 配置文件读取器类。
 * @details 将 INI 文件读取到易于访问的名称/值对中。
 */
    class INIReader {
    public:
        /**
         * @brief 默认构造函数。
         * @details 创建一个空的 INIReader 对象。
         */
        INIReader() {
        };

        /**
         * @brief 构造 INIReader 并解析给定的文件名。
         * @details 关于解析的更多信息，请参见 ini.h。
         * @param filename 要解析的 INI 文件名。
         */
        explicit INIReader(const std::string &filename);

        /**
         * @brief 构造 INIReader 并解析给定的文件流。
         * @details 关于解析的更多信息，请参见 ini.h。
         * @param file 要解析的 INI 文件流。
         */
        explicit INIReader(FILE *file);

        /**
         * @brief 返回 ini_parse() 的结果。
         * @details 即，成功时返回 0，解析错误时返回第一个错误所在的行号，文件打开错误时返回 -1。
         * @return 解析错误代码，0 表示成功，负数表示错误，正数表示解析错误所在的行号。
         */
        int ParseError() const;

        /**
         * @brief 返回在 ini 文件中找到的节列表。
         * @return 包含节名称的常量集合。
         */
        const std::set<std::string> &Sections() const;

        /**
         * @brief 从 INI 文件中获取字符串值。
         * @details 如果未找到，则返回 default_value。
         * @param section 节名。
         * @param name 名称（键名）。
         * @param default_value 默认值，当配置项不存在时返回。
         * @return 找到的字符串值，如果未找到则返回 default_value。
         */
        std::string Get(const std::string &section, const std::string &name,
                        const std::string &default_value) const;

        /**
         * @brief 从 INI 文件中获取整数 (long) 值。
         * @details 如果未找到或不是有效的整数（十进制 "1234"，"-1234" 或十六进制 "0x4d2"），则返回 default_value。
         * @param section 节名。
         * @param name 名称（键名）。
         * @param default_value 默认值，当配置项不存在或无效时返回。
         * @return 找到的整数值，如果未找到或无效则返回 default_value。
         */
        long GetInteger(const std::string &section, const std::string &name,
                        long default_value) const;

        /**
         * @brief 从 INI 文件中获取实数（浮点 double）值。
         * @details 如果未找到或不是有效的浮点数值（根据 strtod()），则返回 default_value。
         * @param section 节名。
         * @param name 名称（键名）。
         * @param default_value 默认值，当配置项不存在或无效时返回。
         * @return 找到的浮点数值，如果未找到或无效则返回 default_value。
         */
        double GetReal(const std::string &section, const std::string &name,
                       double default_value) const;

        /**
         * @brief 从 INI 文件中获取单精度浮点数值 (float)。
         * @details 如果未找到或不是有效的浮点数值（根据 strtof()），则返回 default_value。
         * @param section 节名。
         * @param name 名称（键名）。
         * @param default_value 默认值，当配置项不存在或无效时返回。
         * @return 找到的单精度浮点数值，如果未找到或无效则返回 default_value。
         */
        float GetFloat(const std::string &section, const std::string &name,
                       float default_value) const;

        /**
         * @brief 从 INI 文件中获取布尔值。
         * @details 如果未找到或不是有效的 true/false 值，则返回 default_value。
         *          有效的 true 值包括 "true", "yes", "on", "1"，有效的 false 值包括 "false", "no", "off", "0" (不区分大小写)。
         * @param section 节名。
         * @param name 名称（键名）。
         * @param default_value 默认值，当配置项不存在或无效时返回。
         * @return 找到的布尔值，如果未找到或无效则返回 default_value。
         */
        bool GetBoolean(const std::string &section, const std::string &name,
                        bool default_value) const;

    protected:
        int _error; /**< 解析错误代码 */
        std::map<std::string, std::string> _values; /**< 存储键值对的映射 */
        std::set<std::string> _sections; /**< 存储节名称的集合 */

        /**
         * @brief 创建用于键值对查找的键。
         * @details 将节名和名称组合成一个键，并转换为小写。
         * @param section 节名。
         * @param name 名称（键名）。
         * @return 生成的键字符串。
         */
        static std::string MakeKey(const std::string &section,
                                   const std::string &name);

        /**
         * @brief 值处理函数，用于 ini_parse 的回调。
         * @details 将解析到的值存储到 _values 映射中，并将节名添加到 _sections 集合中。
         * @param user 用户数据指针（INIReader 对象指针）。
         * @param section 节名。
         * @param name 名称（键名）。
         * @param value 值。
         * @return 总是返回 1，表示处理成功。
         */
        static int ValueHandler(void *user, const char *section, const char *name,
                                const char *value);
    };

}

#endif // __INIREADER_H__

#ifndef __INIREADER__
#define __INIREADER__

#include <algorithm>
#include <cctype>
#include <cstdlib>

/**
 * @brief INIReader 类文件名构造函数实现。
 * @param filename INI 文件名。
 */
inline LSX_LIB::Config::INIReader::INIReader(const std::string &filename) {
    _error = ini_parse(filename.c_str(), ValueHandler, this);
}

/**
 * @brief INIReader 类文件流构造函数实现。
 * @param file 文件流指针。
 */
inline LSX_LIB::Config::INIReader::INIReader(FILE *file) {
    _error = ini_parse_file(file, ValueHandler, this);
}

/**
 * @brief ParseError 方法实现。
 * @return 返回解析错误代码。
 */
inline int LSX_LIB::Config::INIReader::ParseError() const { return _error; }

/**
 * @brief Sections 方法实现。
 * @return 返回节名称集合的常量引用。
 */
inline const std::set<std::string> &LSX_LIB::Config::INIReader::Sections() const {
    return _sections;
}

/**
 * @brief Get 方法实现。
 * @param section 节名。
 * @param name 名称（键名）。
 * @param default_value 默认值。
 * @return 返回获取到的字符串值。
 */
inline std::string LSX_LIB::Config::INIReader::Get(const std::string &section,
                                                   const std::string &name,
                                                   const std::string &default_value) const {
    std::string key = MakeKey(section, name);
    return _values.count(key) ? _values.at(key) : default_value;
}

/**
 * @brief GetInteger 方法实现。
 * @param section 节名。
 * @param name 名称（键名）。
 * @param default_value 默认值。
 * @return 返回获取到的整数值。
 */
inline long LSX_LIB::Config::INIReader::GetInteger(const std::string &section,
                                                   const std::string &name,
                                                   long default_value) const {
    std::string valstr = Get(section, name, "");
    const char *value = valstr.c_str();
    char *end;
    // This parses "1234" (decimal) and also "0x4D2" (hex)
    long n = strtol(value, &end, 0);
    return end > value ? n : default_value;
}

/**
 * @brief GetReal 方法实现。
 * @param section 节名。
 * @param name 名称（键名）。
 * @param default_value 默认值。
 * @return 返回获取到的双精度浮点数值。
 */
inline double LSX_LIB::Config::INIReader::GetReal(const std::string &section,
                                                  const std::string &name,
                                                  double default_value) const {
    std::string valstr = Get(section, name, "");
    const char *value = valstr.c_str();
    char *end;
    double n = strtod(value, &end);
    return end > value ? n : default_value;
}

/**
 * @brief GetFloat 方法实现。
 * @param section 节名。
 * @param name 名称（键名）。
 * @param default_value 默认值。
 * @return 返回获取到的单精度浮点数值。
 */
inline float LSX_LIB::Config::INIReader::GetFloat(const std::string &section,
                                                  const std::string &name,
                                                  float default_value) const {
    std::string valstr = Get(section, name, "");
    const char *value = valstr.c_str();
    char *end;
    float n = strtof(value, &end);
    return end > value ? n : default_value;
}

/**
 * @brief GetBoolean 方法实现。
 * @param section 节名。
 * @param name 名称（键名）。
 * @param default_value 默认值。
 * @return 返回获取到的布尔值。
 */
inline bool LSX_LIB::Config::INIReader::GetBoolean(const std::string &section,
                                                   const std::string &name,
                                                   bool default_value) const {
    std::string valstr = Get(section, name, "");
    // Convert to lower case to make string comparisons case-insensitive
    std::transform(valstr.begin(), valstr.end(), valstr.begin(), ::tolower);
    if (valstr == "true" || valstr == "yes" || valstr == "on" || valstr == "1")
        return true;
    else if (valstr == "false" || valstr == "no" || valstr == "off" ||
             valstr == "0")
        return false;
    else
        return default_value;
}

/**
 * @brief MakeKey 方法实现。
 * @param section 节名。
 * @param name 名称（键名）。
 * @return 返回生成的键字符串。
 */
inline std::string LSX_LIB::Config::INIReader::MakeKey(const std::string &section,
                                                       const std::string &name) {
    std::string key = section + "=" + name;
    // Convert to lower case to make section/name lookups case-insensitive
    std::transform(key.begin(), key.end(), key.begin(), ::tolower);
    return key;
}

/**
 * @brief ValueHandler 方法实现。
 * @param user 用户数据指针。
 * @param section 节名。
 * @param name 名称（键名）。
 * @param value 值。
 * @return 总是返回 1。
 */
inline int LSX_LIB::Config::INIReader::ValueHandler(void *user, const char *section,
                                                    const char *name, const char *value) {
    INIReader *reader = (INIReader *) user;
    std::string key = MakeKey(section, name);
    if (reader->_values[key].size() > 0)
        reader->_values[key] += "\n";
    reader->_values[key] += value;
    reader->_sections.insert(section);
    return 1;
}

#endif // __INIREADER__
