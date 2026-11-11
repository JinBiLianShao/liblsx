/**
 * @file test_configstore.cpp
 * @brief ConfigStore 深度测试：INI 读写/类型/绑定/回调/线程安全/边界
 */
#include "test_framework.h"

#include <atomic>
#include <cstdio>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#include "Config/ConfigStore.h"

using namespace LSX_LIB::Config;

static const char* kPath = "/tmp/lsx_test_configstore.ini";

static void writeFile(const std::string& content)
{
    std::ofstream f(kPath);
    f << content;
}

LSX_TEST(ConfigStore, LoadParseAndTypedGet) {
    writeFile(
        "[server]\n"
        "port = 8080\n"
        "host = 127.0.0.1\n"
        "; 注释行\n"
        "# 另一种注释\n"
        "[debug]\n"
        "enabled = yes\n"
        "ratio = 0.75\n");
    ConfigStore cfg;
    LSX_EXPECT_TRUE(cfg.load(kPath).isOk());
    LSX_EXPECT_EQ(cfg.getInt("server", "port", 0), std::int64_t(8080));
    LSX_EXPECT_EQ(cfg.getString("server", "host"), std::string("127.0.0.1"));
    LSX_EXPECT_EQ(cfg.getBool("debug", "enabled", false), true);
    LSX_EXPECT_NEAR(cfg.getDouble("debug", "ratio", 0.0), 0.75, 1e-9);
    LSX_EXPECT_EQ(cfg.size(), 4u);
}

LSX_TEST(ConfigStore, CaseInsensitiveSectionAndKey) {
    writeFile("[Server]\nPort = 9090\n");
    ConfigStore cfg;
    cfg.load(kPath);
    LSX_EXPECT_EQ(cfg.getInt("server", "port", 0), std::int64_t(9090));
    LSX_EXPECT_EQ(cfg.getInt("SERVER", "PORT", 0), std::int64_t(9090));
}

LSX_TEST(ConfigStore, DefaultValuesOnMissingAndInvalid) {
    ConfigStore cfg;
    LSX_EXPECT_EQ(cfg.getString("x", "y", "fallback"), std::string("fallback"));
    LSX_EXPECT_EQ(cfg.getInt("x", "y", 42), std::int64_t(42));
    LSX_EXPECT_NEAR(cfg.getDouble("x", "y", 3.14), 3.14, 1e-9);
    LSX_EXPECT_EQ(cfg.getBool("x", "y", true), true);
    cfg.set("x", "bad_int", std::string("not_a_number"));
    LSX_EXPECT_EQ(cfg.getInt("x", "bad_int", 7), std::int64_t(7));
}

LSX_TEST(ConfigStore, BoolVariants) {
    ConfigStore cfg;
    cfg.set("s", "a", std::string("TRUE"));
    cfg.set("s", "b", std::string("Off"));
    cfg.set("s", "c", std::string("1"));
    cfg.set("s", "d", std::string("0"));
    LSX_EXPECT_EQ(cfg.getBool("s", "a", false), true);
    LSX_EXPECT_EQ(cfg.getBool("s", "b", true), false);
    LSX_EXPECT_EQ(cfg.getBool("s", "c", false), true);
    LSX_EXPECT_EQ(cfg.getBool("s", "d", true), false);
}

LSX_TEST(ConfigStore, TemplateGetSet) {
    ConfigStore cfg;
    cfg.set("s", "i", 123);
    cfg.set("s", "d", 2.5);
    cfg.set("s", "b", true);
    cfg.set("s", "str", std::string("hello"));
    LSX_EXPECT_EQ(cfg.get<int>("s", "i"), 123);
    LSX_EXPECT_NEAR(cfg.get<double>("s", "d"), 2.5, 1e-9);
    LSX_EXPECT_EQ(cfg.get<bool>("s", "b", false), true);
    LSX_EXPECT_EQ(cfg.get<std::string>("s", "str"), std::string("hello"));
    LSX_EXPECT_EQ(cfg.get<std::string>("s", "missing", "def"), std::string("def"));
}

LSX_TEST(ConfigStore, SaveRoundtrip) {
    ConfigStore cfg;
    cfg.set("server", "port", 8080);
    cfg.set("server", "host", std::string("localhost"));
    cfg.set("debug", "enabled", false);
    cfg.set("debug", "ratio", 1.25);
    LSX_EXPECT_TRUE(cfg.save(kPath).isOk());

    ConfigStore cfg2;
    LSX_EXPECT_TRUE(cfg2.load(kPath).isOk());
    LSX_EXPECT_EQ(cfg2.getInt("server", "port"), std::int64_t(8080));
    LSX_EXPECT_EQ(cfg2.getString("server", "host"), std::string("localhost"));
    LSX_EXPECT_EQ(cfg2.getBool("debug", "enabled", true), false);
    LSX_EXPECT_NEAR(cfg2.getDouble("debug", "ratio"), 1.25, 1e-9);
}

LSX_TEST(ConfigStore, SaveProducesValidIniWithSections) {
    ConfigStore cfg;
    cfg.set("net", "ip", std::string("1.2.3.4"));
    cfg.set("app", "name", std::string("lsx"));
    std::string text = cfg.toString();
    LSX_EXPECT_TRUE(text.find("[net]") != std::string::npos);
    LSX_EXPECT_TRUE(text.find("[app]") != std::string::npos);
    LSX_EXPECT_TRUE(text.find("ip = 1.2.3.4") != std::string::npos);
}

LSX_TEST(ConfigStore, BindSyncsOnLoadAndSet) {
    int g_port = 0;
    bool g_debug = true;
    std::string g_host;
    ConfigStore cfg;
    cfg.bind("server", "port", &g_port, 80);
    LSX_EXPECT_EQ(g_port, 80);
    cfg.bind("debug", "enabled", &g_debug, false);
    LSX_EXPECT_EQ(g_debug, false);
    cfg.bind("server", "host", &g_host, std::string("default"));

    writeFile("[server]\nport = 9000\nhost = 10.0.0.1\n[debug]\nenabled = on\n");
    cfg.load(kPath);
    LSX_EXPECT_EQ(g_port, 9000);
    LSX_EXPECT_EQ(g_debug, true);
    LSX_EXPECT_EQ(g_host, std::string("10.0.0.1"));

    cfg.set("server", "port", 7000);
    LSX_EXPECT_EQ(g_port, 7000);
}

LSX_TEST(ConfigStore, BindTypedVariables) {
    long g_long = 0;
    double g_double = 0;
    ConfigStore cfg;
    cfg.bind("n", "l", &g_long, 0L);
    cfg.bind("n", "d", &g_double, 0.0);
    cfg.set("n", "l", static_cast<std::int64_t>(1234567890));
    cfg.set("n", "d", 3.14159);
    LSX_EXPECT_EQ(g_long, 1234567890L);
    LSX_EXPECT_NEAR(g_double, 3.14159, 1e-9);
}

LSX_TEST(ConfigStore, UnbindStopsSync) {
    int g = 0;
    ConfigStore cfg;
    cfg.bind("s", "v", &g, 5);
    cfg.set("s", "v", 10);
    LSX_EXPECT_EQ(g, 10);
    cfg.unbind("s", "v");
    cfg.set("s", "v", 20);
    LSX_EXPECT_EQ(g, 10);
}

LSX_TEST(ConfigStore, ChangeCallbackFiresOnSet) {
    ConfigStore cfg;
    std::vector<ChangeEvent> events;
    auto id = cfg.onChange([&](const ChangeEvent& e) { events.push_back(e); });
    LSX_EXPECT_TRUE(id > 0u);
    cfg.set("s", "k", 1);
    cfg.set("s", "k", 1);
    cfg.set("s", "k", 2);
    LSX_EXPECT_EQ(events.size(), 2u);
    LSX_EXPECT_EQ(events[0].new_value, std::string("1"));
    LSX_EXPECT_EQ(events[1].old_value, std::string("1"));
    LSX_EXPECT_EQ(events[1].new_value, std::string("2"));
    cfg.removeOnChange(id);
    cfg.set("s", "k", 3);
    LSX_EXPECT_EQ(events.size(), 2u);
}

LSX_TEST(ConfigStore, RemoveFiresCallbackAndFailsOnMissing) {
    ConfigStore cfg;
    int fired = 0;
    cfg.onChange([&](const ChangeEvent&) { ++fired; });
    cfg.set("s", "k", 1);
    LSX_EXPECT_TRUE(cfg.remove("s", "k"));
    LSX_EXPECT_TRUE(!cfg.remove("s", "k"));
    LSX_EXPECT_EQ(fired, 2);
}

LSX_TEST(ConfigStore, QueryHelpers) {
    writeFile("[a]\nx = 1\ny = 2\n[b]\nz = 3\n");
    ConfigStore cfg;
    cfg.load(kPath);
    LSX_EXPECT_TRUE(cfg.has("a", "x"));
    LSX_EXPECT_TRUE(!cfg.has("a", "nope"));
    LSX_EXPECT_TRUE(cfg.hasSection("a"));
    LSX_EXPECT_TRUE(cfg.hasSection("b"));
    LSX_EXPECT_EQ(cfg.sections().size(), 2u);
    LSX_EXPECT_EQ(cfg.keys("a").size(), 2u);
    cfg.clear();
    LSX_EXPECT_EQ(cfg.size(), 0u);
    LSX_EXPECT_TRUE(!cfg.has("a", "x"));
}

LSX_TEST(ConfigStore, LoadErrorsAreReported) {
    ConfigStore cfg;
    LSX_EXPECT_TRUE(cfg.load("/tmp/definitely_no_such_config_file.ini").isErr());
    LSX_EXPECT_TRUE(cfg.loadFromString("orphan_key = 1\n").isErr());
    LSX_EXPECT_TRUE(cfg.loadFromString("[]\nk = 1\n").isErr());
    LSX_EXPECT_TRUE(cfg.loadFromString("[s]\nno_equals_sign\n").isErr());
}

LSX_TEST(ConfigStore, InlineCommentsStripped) {
    writeFile("[s]\nport = 8080 ; 服务端口\nname = abc # 备注\n");
    ConfigStore cfg;
    cfg.load(kPath);
    LSX_EXPECT_EQ(cfg.getInt("s", "port"), std::int64_t(8080));
    LSX_EXPECT_EQ(cfg.getString("s", "name"), std::string("abc"));
}

LSX_TEST(ConfigStore, HexAndNegativeIntegers) {
    writeFile("[s]\nhex = 0x4d2\nneg = -256\n");
    ConfigStore cfg;
    cfg.load(kPath);
    LSX_EXPECT_EQ(cfg.getInt("s", "hex"), std::int64_t(1234));
    LSX_EXPECT_EQ(cfg.getInt("s", "neg"), std::int64_t(-256));
}

LSX_TEST(ConfigStore, ConcurrentReadersDuringWrites) {
    ConfigStore cfg;
    cfg.set("s", "v", 0);
    std::atomic<bool> stop{false};
    std::vector<std::thread> readers;
    for (int t = 0; t < 4; ++t)
        readers.emplace_back([&]() {
            while (!stop.load()) (void)cfg.getInt("s", "v", 0);
        });
    for (int i = 0; i < 1000; ++i) cfg.set("s", "v", static_cast<std::int64_t>(i));
    stop = true;
    for (auto& r : readers) r.join();
    LSX_EXPECT_EQ(cfg.getInt("s", "v"), std::int64_t(999));
}

LSX_TEST(ConfigStore, INIReaderCompatibleGet) {
    ConfigStore cfg;
    cfg.set("s", "k", std::string("v"));
    LSX_EXPECT_EQ(cfg.Get("s", "k", "def"), std::string("v"));
    LSX_EXPECT_EQ(cfg.Get("s", "missing", "def"), std::string("def"));
}
