/**
 * @file test_main.cpp
 * @brief 测试入口：运行注册的全部用例，失败返回非零
 */
#include "test_framework.h"

int main() {
    std::printf("\n========= LIBLSX 深度测试套件 =========\n");
    return lsx_test::run_all();
}
