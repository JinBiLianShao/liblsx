# arm32 (arm-linux-gnueabihf) 交叉编译 toolchain
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(ARM32_TOOLCHAIN_PREFIX "/opt/gcc-linaro-7.5.0-2019.12-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf"
    CACHE STRING "arm-linux-gnueabihf toolchain prefix")

set(CMAKE_C_COMPILER "${ARM32_TOOLCHAIN_PREFIX}-gcc")
set(CMAKE_CXX_COMPILER "${ARM32_TOOLCHAIN_PREFIX}-g++")

set(CMAKE_FIND_ROOT_PATH /opt/gcc-linaro-7.5.0-2019.12-x86_64_arm-linux-gnueabihf/arm-linux-gnueabihf/libc)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
