/**
 * @file test_config.cpp
 * @brief Config 模块测试：INIReader 解析/默认值/边界
 */
#include "test_framework.h"

#include <cmath>
#include <cstdio>
#include <fstream>

#include "INIReader.h"

using namespace LSX_LIB;

static const char* kIniPath = "/tmp/lsx_test_config.ini";

static void write_ini(const char* content) {
    std::ofstream f(kIniPath);
    f << content;
}

LSX_TEST(INIReader, ParseBasicTypes) {
    write_ini(
        "[server]\n"
        "port = 8080\n"
        "host = 192.168.1.1\n"
        "rate = 0.5\n"
        "verbose = true\n"
        "[debug]\n"
        "enabled = on\n");
    Config::INIReader ini(kIniPath);
    LSX_EXPECT_TRUE(ini.ParseError() == 0);
    LSX_EXPECT_EQ(ini.GetInteger("server", "port", 0), 8080);
    LSX_EXPECT_EQ(ini.Get("server", "host", ""), std::string("192.168.1.1"));
    LSX_EXPECT_TRUE(std::fabs(ini.GetReal("server", "rate", 0.0) - 0.5) < 1e-9);
    LSX_EXPECT_EQ(ini.GetBoolean("server", "verbose", false), true);
    LSX_EXPECT_EQ(ini.GetBoolean("debug", "enabled", false), true);   // on 变体
}

LSX_TEST(INIReader, DefaultValuesOnMissing) {
    Config::INIReader ini(kIniPath);
    LSX_EXPECT_EQ(ini.GetInteger("server", "not_exist", 9999), 9999);
    LSX_EXPECT_EQ(ini.Get("server", "not_exist", "fallback"), std::string("fallback"));
    LSX_EXPECT_EQ(ini.GetBoolean("server", "not_exist", true), true);
}

LSX_TEST(INIReader, SectionNameCaseInsensitive) {
    write_ini("[SERVER]\nport = 9090\n");
    Config::INIReader ini(kIniPath);
    LSX_EXPECT_EQ(ini.GetInteger("server", "port", 0), 9090);   // 常见约定：section 不区分大小写
}

LSX_TEST(INIReader, CommentsAndBlankLinesIgnored) {
    write_ini(
        "; 行注释\n"
        "# 井号注释\n"
        "\n"
        "[server]  \n"
        "port = 7000 ; 行尾注释\n");
    Config::INIReader ini(kIniPath);
    LSX_EXPECT_EQ(ini.GetInteger("server", "port", 0), 7000);
}

LSX_TEST(INIReader, NonexistentFileReportsError) {
    Config::INIReader ini("/tmp/lsx_no_such_file_definitely.ini");
    LSX_EXPECT_TRUE(ini.ParseError() < 0);   // 文件打开失败返回负数
}

LSX_TEST(INIReader, MissingValueWithEmptyDefault) {
    Config::INIReader ini(kIniPath);
    write_ini("[a]\nb = 1\n");
    Config::INIReader ini2(kIniPath);
    LSX_EXPECT_EQ(ini2.Get("a", "missing", ""), std::string(""));
    LSX_EXPECT_EQ(ini2.GetInteger("a", "b", 0), 1);
}
