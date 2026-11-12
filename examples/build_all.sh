#!/usr/bin/env bash
# =====================================================================
# LIBLSX 示例一键构建脚本（独立于顶层 CMake）
#
# 用法：cd examples && ./build_all.sh
# 步骤：1) 用 g++ 把库静态库编出来；2) 逐个把示例编成可执行文件到 ./bin/
# 产物：./liblsx.a  与  ./bin/example_<模块>
# =====================================================================
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SRC_DIR="$ROOT/src"
INC_DIR="$ROOT/Include"
BIN_DIR="$ROOT/examples/bin"
CXX="${CXX:-g++}"
CXXFLAGS="-std=c++17 -O2 -Wall -Wextra"

# 库源码用扁平 include（如 #include "CommunicationFactory.h"），
# 需把 Include 下每个模块子目录都加进头文件搜索路径（与顶层 CMake 一致）。
ALL_INC="-I$INC_DIR"
for d in "$INC_DIR"/*/; do
    ALL_INC="$ALL_INC -I$d"
done

mkdir -p "$BIN_DIR"

echo "[1/3] 编译静态库 liblsx.a ..."
SRC_FILES=$(find "$SRC_DIR" -name "*.cpp" -o -name "*.c" | sort)
SRC_FILES=$(echo "$SRC_FILES" | grep -v "miniz_zip.c" || true)

OBJ_FILES=()
for f in $SRC_FILES; do
    obj="$BIN_DIR/$(basename "${f%.*}").o"
    "$CXX" $CXXFLAGS -DMINIZ_NO_ARCHIVE_APIS -DMINIZ_NO_STDIO \
        $ALL_INC -I"$INC_DIR/third_party/tiny-AES" -I"$INC_DIR/third_party/miniz" \
        -c "$f" -o "$obj"
    OBJ_FILES+=("$obj")
done
ar rcs "$ROOT/examples/liblsx.a" "${OBJ_FILES[@]}"
echo "      静态库已生成: $ROOT/examples/liblsx.a"

echo "[2/3] 编译模块示例 ..."
for src in "$ROOT"/examples/example_*.cpp; do
    name=$(basename "$src" .cpp)
    "$CXX" $CXXFLAGS $ALL_INC "$src" "$ROOT/examples/liblsx.a" -lpthread -o "$BIN_DIR/$name"
    echo "      已编译: bin/$name"
done

echo "[3/3] 完成。运行示例："
echo "      $BIN_DIR/example_timer"
