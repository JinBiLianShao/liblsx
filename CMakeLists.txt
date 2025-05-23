cmake_minimum_required(VERSION 3.10)

#发布模式
set(CMAKE_BUILD_TYPE Release)
#set(CMAKE_BUILD_TYPE Debug)

# 检测并配置OpenMP支持
#find_package(OpenMP REQUIRED)
#if (OPENMP_FOUND)
#    message("OpenMP support found.")
#    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
#    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
#    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")
#endif()

# 设置C++标准为C++14
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# 设置项目名称
set(PROJECT_NAME "lsx") #改成自己的项目名称
project(${PROJECT_NAME})

set(COMPILER "arm64") #模式选项

if (COMPILER MATCHES "arm64")
    set(CMAKE_HOST_SYSTEM_NAME Linux)
    set(CMAKE_C_COMPILER "/usr/bin/aarch64-linux-gnu-gcc-7")
    set(CMAKE_CXX_COMPILER "/usr/bin/aarch64-linux-gnu-g++-7")
    link_directories(./Include/SQL/SQLite3/sqlite3-arm64/lib) #arm64 sqlite3
elseif(COMPILER MATCHES "arm32")
    set(CMAKE_HOST_SYSTEM_NAME Linux)
    set(CMAKE_C_COMPILER "/usr/bin/arm-linux-gnueabihf-gcc-7")
    set(CMAKE_CXX_COMPILER "/usr/bin/arm-linux-gnueabihf-g++-7")
    #set(CMAKE_C_COMPILER "/home/lsx/gcc-linaro-5.4.1-2017.05-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-gcc")
    #set(CMAKE_CXX_COMPILER "/home/lsx/gcc-linaro-5.4.1-2017.05-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-g++")
    #set(CMAKE_CXX_LINK_EXECUTABLE "/home/lsx/gcc-linaro-5.4.1-2017.05-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-ld")
    link_directories(./Include/SQL/SQLite3/sqlite3-arm32/lib) #arm32 sqlite3
elseif(COMPILER MATCHES "x86")
    set(CMAKE_C_COMPILER "gcc")
    set(CMAKE_CXX_COMPILER "g++")
    link_directories(./Include/SQL/SQLite3/sqlite3-x86/lib) #x86 sqlite3
endif()


# 自动查找所有源文件并添加到目标
file(GLOB_RECURSE SOURCES "${CMAKE_SOURCE_DIR}/*.cpp" "${CMAKE_SOURCE_DIR}/*.c" "${CMAKE_SOURCE_DIR}/*.cc")
# 手动排除构建目录中的生成文件
list(FILTER SOURCES EXCLUDE REGEX "${CMAKE_BINARY_DIR}")
add_library(${PROJECT_NAME}
        #STATIC  #静态库
        SHARED #动态库
        ${SOURCES})
#add_executable(${PROJECT_NAME} ${SOURCES})


# 自动查找所有头文件并获取其目录
file(GLOB_RECURSE HEADER_FILES "${CMAKE_SOURCE_DIR}/*.h" "${CMAKE_SOURCE_DIR}/*.hpp")

# 提取目录路径
set(INCLUDE_DIRS "")
foreach(HEADER_FILE ${HEADER_FILES})
    get_filename_component(DIR_PATH ${HEADER_FILE} DIRECTORY)
    list(APPEND INCLUDE_DIRS ${DIR_PATH})
endforeach()

# 只保留唯一的目录
list(REMOVE_DUPLICATES INCLUDE_DIRS)

# 添加到目标
target_include_directories(${PROJECT_NAME} PUBLIC ${INCLUDE_DIRS})

# 链接pthread库
target_link_libraries(${PROJECT_NAME} pthread rt sqlite3)
