# =====================================================================
# LIBLSX 示例集构建文件
#
# 用法（在库根目录下，把示例集挂进顶层构建）：
#   mkdir -p build && cd build
#   cmake .. -DCOMPILER_TARGET=x86 -DTARGET_TYPE=both -DBUILD_EXAMPLES=ON
#   make example_all          # 一键构建全部 17 个模块示例
#
# 产物：build/x86/bin/example_<module>
#
# 更简单的独立用法（不依赖顶层 CMake，直接 g++ 编单个示例）：
#   cd examples && ./build_all.sh   （见 README.md）
# =====================================================================
cmake_minimum_required(VERSION 3.10)

# INCLUDE_DIRS（头文件搜索路径）、PROJECT_NAME 由顶层 CMakeLists.txt 传入
if (NOT DEFINED INCLUDE_DIRS)
    message(FATAL_ERROR "INCLUDE_DIRS 未定义：请通过顶层 CMakeLists.txt add_subdirectory(examples) 引入")
endif ()

# 收集本目录下所有 example_*.cpp
file(GLOB EXAMPLE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/example_*.cpp")

# 为每个示例建一个可执行目标
foreach (src ${EXAMPLE_SOURCES})
    get_filename_component(name "${src}" NAME_WE)      # example_timer
    string(REPLACE "example_" "" tgt "${name}")         # timer
    add_executable(example_${tgt} "${src}")
    target_include_directories(example_${tgt} PRIVATE ${INCLUDE_DIRS})
    target_link_libraries(example_${tgt} PRIVATE ${PROJECT_NAME}_static Threads::Threads)
    if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(example_${tgt} PRIVATE -Wall -Wextra)
    endif ()
endforeach ()

# 汇总目标：一键构建所有示例
add_custom_target(example_all DEPENDS
    example_bus example_checksum example_compress example_config
    example_crypto example_datatransfer example_eventbus example_ipc
    example_lock example_logger example_memory example_protocol
    example_result example_serialize example_statemachine
    example_thread example_timer)

message(STATUS "示例集已配置：17 个模块示例，目标 example_all 一键构建")
