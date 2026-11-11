# Third-party vendored sources

本目录存放库内 vendor 的第三方源码，全部为宽松许可证，随库一起编译，
目标板上无需额外安装动态库（保持 arm32/arm64 交叉编译自包含）。

| 组件 | 版本/来源 | 许可证 | 用途 |
|------|-----------|--------|------|
| `tiny-AES/aes.h` + `src/third_party/tiny-AES/aes.c` | kokke/tiny-AES-c (master) | Unlicense (公有领域) | AES-128/192/256 ECB/CBC/CTR |
| `miniz/miniz.h` + `src/third_party/miniz/miniz*.c` | richgel999/miniz 3.1.2 | MIT | DEFLATE/zlib 压缩解压 |
| `Http/httplib.h`, `Http/json.hpp` | 上游单文件头 | MIT | HTTP / JSON（既有） |
| `SQL/SQLite3/*` | SQLite 官方预编译 | Public Domain | SQLite（既有） |

## 本地适配说明

- **AES 三实例**：tiny-AES-c 用编译期宏决定密钥长度，一份源码只能有一种。
  `src/third_party/tiny-AES/aes_variants.cpp` 通过 namespace 包裹 + 宏 undef 重展开，
  把上游源码编译成 128/192/256 三套符号，对外由 `Include/Crypto/Aes.h` 按密钥长度分发。
  `Include/third_party/tiny-AES/aes.h` 仅把上游写死的 `#define AES128 1` 改为可被外部覆盖。
- **miniz 拆分版**：3.1.2 把实现拆成 `miniz.c` / `miniz_tdef.c` / `miniz_tinfl.c`，
  三者均随库编译；`miniz_zip.c`（ZIP 归档）未引入，并定义 `MINIZ_NO_ARCHIVE_APIS`。
  `miniz_export.h` 是本库补的静态链接适配头（上游由 CMake 生成，用于 DLL 导出）。
  完整源码经 SSH clone 上游仓库获取（web fetch 对大文件有截断）。

AES 与 miniz 的 C 源码仅参与库自身编译，不对外暴露 C API；
C++ 封装见 `Include/Crypto/` 与 `Include/Compress/`。
