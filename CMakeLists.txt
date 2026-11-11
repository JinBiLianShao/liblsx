cmake_minimum_required(VERSION 3.10)

# LIBLSX 构建系统 v3.0
# 作者: 连思鑫（jinbilianshao）  更新: 2026-11-10
#
# 功能：一个构建同时产出静态库与动态库；多平台交叉编译（x86/arm32/arm64，经 toolchain 文件）；
#       产出物按平台归档 build/<target>/{static,shared}/；SOVERSION 版本软链；
#       make install 分发头文件与库（vendored 预编译产物排除）；警告基线与 OpenMP 开关。
#
# 选项（cmake -D<选项>=<值>）：
#   TARGET_TYPE       both(默认) | static | shared | executable
#   COMPILER_TARGET   x86(默认) | arm32 | arm64   —— 仅供产出物归档命名
#   ENABLE_OPENMP     OFF(默认) | ON
#
# 交叉编译：编译器选择属于 toolchain 文件职责（toolchains/*.cmake），本文件不再硬编码编译器路径：
#   mkdir -p build/arm64 && cd build/arm64
#   cmake <repo根> -DCMAKE_TOOLCHAIN_FILE=<repo根>/toolchains/arm64.cmake -DCOMPILER_TARGET=arm64 -DTARGET_TYPE=both

# 项目名称、版本与语言
if (NOT DEFINED PROJECT_NAME)
    set(PROJECT_NAME "lsx")
endif ()
project(${PROJECT_NAME}
        VERSION 0.5.0
        DESCRIPTION "LIBLSX - lightweight cross-platform C++ utility library"
        LANGUAGES C CXX)

# 构建类型默认值
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type (Release or Debug)" FORCE)
endif ()

# 编译标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ===================== 用户配置变量 =====================
set(COMPILER_TARGET "x86" CACHE STRING "Target architecture label: x86 | arm32 | arm64")
set(TARGET_TYPE "both" CACHE STRING "Build target type: both | static | shared | executable")
option(ENABLE_OPENMP "Enable OpenMP parallelism" OFF)
# ========================================================

# ===================== 目标平台归档目录 =====================
# 产出物统一归档到 build/<COMPILER_TARGET>/ 下，多平台构建互不覆盖
set(PLATFORM_OUTPUT_DIR "${CMAKE_BINARY_DIR}/${COMPILER_TARGET}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${PLATFORM_OUTPUT_DIR}/static")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PLATFORM_OUTPUT_DIR}/shared")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PLATFORM_OUTPUT_DIR}/bin")

# ===================== 源文件与头文件收集 =====================
# 只收集库自身源码；Include/ 下的 vendored 头（httplib/json/sqlite）是给下游用的，不参与编译
# v0.5.0 起 src/third_party/ 下的 vendor C 源码（AES/miniz）随库一起编译，自包含无外部 .so
file(GLOB_RECURSE SRC_FILES "${CMAKE_SOURCE_DIR}/src/*.cpp")
# vendor 的 AES/miniz 实现含 .c 文件，一并收集（作为 C++ 编译，与库内其它源码一致）
file(GLOB_RECURSE SRC_C_FILES "${CMAKE_SOURCE_DIR}/src/*.c")
list(APPEND SRC_FILES ${SRC_C_FILES})
# miniz 3.x 把 ZIP 归档实现单独放在 miniz_zip.c；本库只用 DEFLATE，
# 排除它以减小体积并避免归档符号进入库（需保留 miniz.c/tdef/tinfl）
list(FILTER SRC_FILES EXCLUDE REGEX ".*/miniz_zip\.c$")
add_compile_definitions(MINIZ_NO_ARCHIVE_APIS MINIZ_NO_STDIO)
# vendor miniz 在 32 位平台有两处恒假比较（size_t 范围导致），仅对 vendor 文件静音该警告
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set_source_files_properties(
        ${SRC_C_FILES}
        PROPERTIES COMPILE_OPTIONS "-Wno-type-limits")
endif ()
list(FILTER SRC_FILES EXCLUDE REGEX ".*/CMakeFiles/.*")

set(INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/Include")
# vendor C 头目录（AES/miniz），仅供库内 .cpp 编译使用
list(APPEND INCLUDE_DIRS
        "${CMAKE_SOURCE_DIR}/Include/third_party/tiny-AES"
        "${CMAKE_SOURCE_DIR}/Include/third_party/miniz")
# 头文件采用跨模块扁平 include（如 #include "ThreadPool.h"），需把各模块目录都加进路径
file(GLOB_RECURSE LIB_HEADERS "${CMAKE_SOURCE_DIR}/Include/*.h" "${CMAKE_SOURCE_DIR}/Include/*.hpp")
foreach (_hdr ${LIB_HEADERS})
    get_filename_component(_dir "${_hdr}" DIRECTORY)
    list(APPEND INCLUDE_DIRS "${_dir}")
endforeach ()
list(REMOVE_DUPLICATES INCLUDE_DIRS)

# ===================== 构建目标定义 =====================
set(LSX_TARGETS "")

function(lsx_setup_target tgt)
    target_include_directories(${tgt} PUBLIC ${INCLUDE_DIRS})
    target_compile_definitions(${tgt} PRIVATE LSX_LIB_VERSION="${PROJECT_VERSION}")

    # 统一编译警告（库自身代码的质量基线）
    if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${tgt} PRIVATE -Wall -Wextra -Wpedantic)
    endif ()

    # 动态库需要 -fPIC；静态库可能被链入动态库，统一开启
    set_target_properties(${tgt} PROPERTIES
            POSITION_INDEPENDENT_CODE ON)
endfunction()

if (TARGET_TYPE STREQUAL "static" OR TARGET_TYPE STREQUAL "both")
    add_library(${PROJECT_NAME}_static STATIC ${SRC_FILES})
    set_target_properties(${PROJECT_NAME}_static PROPERTIES
            OUTPUT_NAME "${PROJECT_NAME}"
            VERSION ${PROJECT_VERSION})
    lsx_setup_target(${PROJECT_NAME}_static)
    list(APPEND LSX_TARGETS ${PROJECT_NAME}_static)
endif ()

if (TARGET_TYPE STREQUAL "shared" OR TARGET_TYPE STREQUAL "both")
    add_library(${PROJECT_NAME}_shared SHARED ${SRC_FILES})
    set_target_properties(${PROJECT_NAME}_shared PROPERTIES
            OUTPUT_NAME "${PROJECT_NAME}"
            VERSION ${PROJECT_VERSION}
            SOVERSION ${PROJECT_VERSION_MAJOR})
    lsx_setup_target(${PROJECT_NAME}_shared)
    list(APPEND LSX_TARGETS ${PROJECT_NAME}_shared)
endif ()

if (TARGET_TYPE STREQUAL "executable")
    add_executable(${PROJECT_NAME} ${SRC_FILES})
    lsx_setup_target(${PROJECT_NAME})
    target_link_libraries(${PROJECT_NAME} PRIVATE pthread rt dl m)
    list(APPEND LSX_TARGETS ${PROJECT_NAME})
endif ()

if (NOT LSX_TARGETS)
    message(FATAL_ERROR "Invalid TARGET_TYPE: ${TARGET_TYPE} (must be both | static | shared | executable)")
endif ()

# ===================== 线程库链接 =====================
# 库自身用到 std::thread；静态库消费者需要线程库，通过 PUBLIC 传递
find_package(Threads REQUIRED)
foreach (tgt ${LSX_TARGETS})
    target_link_libraries(${tgt} PUBLIC Threads::Threads)
endforeach ()

# ===================== OpenMP 支持（可选） =====================
if (ENABLE_OPENMP)
    find_package(OpenMP REQUIRED)
    foreach (tgt ${LSX_TARGETS})
        target_compile_options(${tgt} PRIVATE ${OpenMP_CXX_FLAGS})
        target_link_libraries(${tgt} PUBLIC ${OpenMP_CXX_LIBRARIES})
    endforeach ()
endif ()

# ===================== 安装规则（make install 分发头文件与库） =====================
include(GNUInstallDirs)
install(TARGETS ${LSX_TARGETS}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY Include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME}
        FILES_MATCHING
        PATTERN "*.h" PATTERN "*.hpp"
        PATTERN "SQL/SQLite3/sqlite3-*" EXCLUDE)

# ===================== 测试套件（可选） =====================
option(ENABLE_TESTS "Build and register the test suite" OFF)
if (ENABLE_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif ()

# ===================== 构建信息汇总 =====================
list(LENGTH SRC_FILES SRC_COUNT)
message(STATUS "")
message(STATUS "=========== LIBLSX 构建配置 ===========")
message(STATUS "  项目        : ${PROJECT_NAME} v${PROJECT_VERSION}")
message(STATUS "  目标平台    : ${COMPILER_TARGET}")
message(STATUS "  构建类型    : ${CMAKE_BUILD_TYPE}")
message(STATUS "  目标产物    : ${TARGET_TYPE}")
message(STATUS "  C++ 编译器  : ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  源文件数    : ${SRC_COUNT}")
message(STATUS "  产出目录    : ${PLATFORM_OUTPUT_DIR}")
message(STATUS "  OpenMP      : ${ENABLE_OPENMP}")
message(STATUS "========================================")
message(STATUS "")
