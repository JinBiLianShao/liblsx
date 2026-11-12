#!/usr/bin/env bash
# =====================================================================
# LIBLSX 一键跨平台编译脚本
#
# 功能：一次编译 x86 / arm32 / arm64 三平台的静态库、动态库（或其一）。
# 产物归档在 build/<平台>/<平台>/{static,shared}/ 下（外层目录是 CMake 二进制目录，
#
# 用法：
#   ./build_all.sh                      # 默认：三平台 both（静态 + 动态）
#   ./build_all.sh -p x86               # 只编 x86
#   ./build_all.sh -p x86,arm64         # 编 x86 和 arm64
#   ./build_all.sh -t static            # 只编静态库
#   ./build_all.sh -t shared            # 只编动态库
#   ./build_all.sh -p arm32 -t both     # 组合使用
#   ./build_all.sh --tests              # 编完跑测试（x86）
#   ./build_all.sh --examples           # 顺带编模块示例
#   ./build_all.sh -j8                  # 并行编译 8 线程
#   ./build_all.sh -h                   # 查看帮助
#
# 环境变量（覆盖 toolchain 前缀，见 toolchains/*.cmake）：
#   ARM32_TOOLCHAIN_PREFIX=/path/to/arm-linux-gnueabihf
#   ARM64_TOOLCHAIN_PREFIX=/path/to/aarch64-linux-gnu
# =====================================================================

set -euo pipefail

# ---- 脚本自身定位（可从任意目录调用）----
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT"

# ---- 默认参数 ----
PLATFORMS="x86,arm32,arm64"
TARGET_TYPE="both"
JOBS="$(nproc 2>/dev/null || echo 4)"
RUN_TESTS=0
BUILD_EXAMPLES=0
BUILD_TYPE="Release"

# ---- 颜色输出 ----
if [ -t 1 ]; then
    C_GREEN="\033[32m"; C_YELLOW="\033[33m"; C_RED="\033[31m"; C_BOLD="\033[1m"; C_RESET="\033[0m"
else
    C_GREEN=""; C_YELLOW=""; C_RED=""; C_BOLD=""; C_RESET=""
fi

info()  { echo -e "${C_GREEN}[INFO]${C_RESET} $*"; }
warn()  { echo -e "${C_YELLOW}[WARN]${C_RESET} $*"; }
error() { echo -e "${C_RED}[ERROR]${C_RESET} $*"; }
step()  { echo -e "${C_BOLD}===== $* =====${C_RESET}"; }

# ---- 帮助 ----
usage() {
    sed -n "5,20p" "$0" | sed "s/^# {0,1}//"
    exit 0;
}

# ---- 解析参数 ----
while [ $# -gt 0 ]; do
    case "$1" in
        -p|--platforms)  PLATFORMS="$2"; shift 2 ;;
        -t|--type)       TARGET_TYPE="$2"; shift 2 ;;
        -j*)             JOBS="${1#-j}"; shift ;;
        --tests)         RUN_TESTS=1; shift ;;
        --examples)      BUILD_EXAMPLES=1; shift ;;
        -d|--debug)      BUILD_TYPE="Debug"; shift ;;
        -h|--help)       usage ;;
        *) error "未知参数: $1（-h 查看帮助）"; exit 1 ;;
    esac
done

# ---- 参数校验 ----
case "$TARGET_TYPE" in
    both|static|shared|executable) ;;
    *) error "TARGET_TYPE 必须是 both|static|shared|executable，当前: $TARGET_TYPE"; exit 1 ;;
esac

# ---- 平台 -> toolchain 文件映射 ----
toolchain_for() {
    case "$1" in
        x86)   echo "" ;;                                  # 本机编译，无需 toolchain
        arm32) echo "$ROOT/toolchains/arm32.cmake" ;;
        arm64) echo "$ROOT/toolchains/arm64.cmake" ;;
        *)     return 1 ;;
    esac
}

# ---- 检查交叉编译器是否存在（不存在则跳过该平台）----
compiler_missing() {
    case "$1" in
        x86)   command -v g++ >/dev/null 2>&1 && return 1 || return 0 ;;
        arm32) local pfx="${ARM32_TOOLCHAIN_PREFIX:-/opt/gcc-linaro-7.5.0-2019.12-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf}";
               [ -x "$pfx-g++" ] && return 1 || return 0 ;;
        arm64) local pfx="${ARM64_TOOLCHAIN_PREFIX:-/bin/aarch64-linux-gnu}";
               command -v "$pfx-g++" >/dev/null 2>&1 && return 1 || return 0 ;;
    esac
}

# ---- 单平台编译 ----
build_one() {
    local plat="$1"
    local tc; tc="$(toolchain_for "$plat")" || { error "未知平台: $plat"; return 1; }
    local build_dir="$ROOT/build/$plat"

    step "编译 $plat（$TARGET_TYPE / $BUILD_TYPE）"

    # 组装 cmake 参数
    local cmake_args=(
        "-S" "$ROOT"
        "-B" "$build_dir"
        "-DCOMPILER_TARGET=$plat"
        "-DTARGET_TYPE=$TARGET_TYPE"
        "-DCMAKE_BUILD_TYPE=$BUILD_TYPE"
    )
    if [ -n "$tc" ]; then
        cmake_args+=("-DCMAKE_TOOLCHAIN_FILE=$tc");
    fi
    # toolchain 前缀覆盖（传递给交叉编译）
    if [ "$plat" = "arm32" ] && [ -n "${ARM32_TOOLCHAIN_PREFIX:-}" ]; then
        cmake_args+=("-DARM32_TOOLCHAIN_PREFIX=$ARM32_TOOLCHAIN_PREFIX");
    fi
    if [ "$plat" = "arm64" ] && [ -n "${ARM64_TOOLCHAIN_PREFIX:-}" ]; then
        cmake_args+=("-DARM64_TOOLCHAIN_PREFIX=$ARM64_TOOLCHAIN_PREFIX");
    fi
    # tests / examples 开关（仅 x86 有意义：交叉编译产物无法在本机运行）
    if [ "$plat" = "x86" ]; then
        [ "$RUN_TESTS" = "1" ] && cmake_args+=("-DENABLE_TESTS=ON");
        [ "$BUILD_EXAMPLES" = "1" ] && cmake_args+=("-DBUILD_EXAMPLES=ON");
    fi

    info "cmake 配置 ..."
    cmake "${cmake_args[@]}" >/dev/null || { error "cmake 配置失败（$plat）"; return 1; }

    info "make 编译（-j$JOBS）..."
    cmake --build "$build_dir" -j "$JOBS" || { error "编译失败（$plat）"; return 1; }

    # 打印产物
    echo "  ${C_GREEN}产物：${C_RESET}"
    local f;
    for f in "$build_dir/$plat/static"/*.a "$build_dir/$plat/shared"/*.so*; do
        [ -e "$f" ] && echo "    - $f";
    done
    echo "";
    return 0;
}

# =====================================================================
# 主流程
# =====================================================================
step "LIBLSX 一键跨平台编译"
echo "  平台     : $PLATFORMS"
echo "  产物类型 : $TARGET_TYPE"
echo "  并行数   : $JOBS"
echo "  构建类型 : $BUILD_TYPE"
echo "  跑测试   : $([ "$RUN_TESTS" = "1" ] && echo 是 || echo 否)"
echo "  编示例   : $([ "$BUILD_EXAMPLES" = "1" ] && echo 是 || echo 否)"
echo ""

FAILED=""
SKIPPED=""
OK=""

IFS="," read -ra PLAT_LIST <<< "$PLATFORMS"
for plat in "${PLAT_LIST[@]}"; do
    plat="$(echo "$plat" | xargs)"   # 去空白
    [ -z "$plat" ] && continue;

    # 工具链缺失 -> 跳过并警告（不中断其余平台）
    if compiler_missing "$plat"; then
        warn "平台 $plat 的编译器不存在，跳过";
        SKIPPED="$SKIPPED $plat";
        continue;
    fi

    if build_one "$plat"; then
        OK="$OK $plat";
    else
        FAILED="$FAILED $plat";
    fi
done

# ---- 测试（仅 x86，交叉编译产物无法在本机执行）----
if [ "$RUN_TESTS" = "1" ] && [[ " $OK " == *" x86 "* ]]; then
    step "运行测试套件（x86）"
    "$ROOT/build/x86/x86/bin/lsx_tests" || { error "测试未全部通过"; FAILED="$FAILED tests"; }
fi

# ---- 汇总 ----
echo ""
step "编译汇总"
[ -n "$OK" ]     && echo -e "${C_GREEN}成功：${C_RESET}$OK"
[ -n "$SKIPPED" ] && echo -e "${C_YELLOW}跳过（缺工具链）：${C_RESET}$SKIPPED"
[ -n "$FAILED" ] && echo -e "${C_RED}失败：${C_RESET}$FAILED"

if [ -n "$FAILED" ]; then
    error "存在失败项，脚本以非零码退出";
    exit 1;
fi

info "全部完成。产物位置：build/<平台>/<平台>/{static,shared}/"
exit 0;
