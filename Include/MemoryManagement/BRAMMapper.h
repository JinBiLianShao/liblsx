/**
 * @file BRAMMapper.h
 * @brief 提供将物理内存映射到用户空间，并支持线程安全的读写操作的功能。
 * @author 连思鑫
 *
 * 该类用于将指定的物理地址区域映射到用户空间，并提供对该内存区域的读写操作。所有的读写操作均为线程安全的，并且类内部使用互斥锁保证数据的一致性。
 * 类中还包含了对内存映射的地址对齐处理，确保映射的起始地址是页面大小的倍数，避免映射时可能出现的内存访问异常。
 *
 * 示例代码：
 * @code
    #include "BRAMMapper.h"
    #include <iostream>
    #include <vector>

    int main() {
        try {
            // 要映射的物理地址和大小
            off_t physicalAddress = 0x40020000;
            size_t mapSize = 12800;

            // 创建 BRAMMapper 实例
            BRAMMapper bram(physicalAddress, mapSize);

            // 读取 12800 字节的数据
            std::vector<uint8_t> data(mapSize);
            for (size_t offset = 0; offset < mapSize; ++offset) {
                data[offset] = bram.read<uint8_t>(offset);
            }

            // 打印部分数据以验证
            for (size_t i = 0; i < 16; ++i) { // 只打印前 16 字节
                std::cout << "Byte " << i << ": 0x" << std::hex << static_cast<int>(data[i]) << std::endl;
            }

        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
        }

        return 0;
    }
 * @endcode
 */

#ifndef BRAM_MAPPER_H
#define BRAM_MAPPER_H
#pragma once
#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdexcept>
#include <cstdint>
#include <mutex>
/**
 * @brief LSX 库的根命名空间。
 */
namespace LIB_LSX::Memory {
/**
 * @class BRAMMapper
 * @brief 提供对物理内存区域的映射及读写操作。
 *
 * 该类将物理内存地址映射到用户空间，并提供线程安全的内存读写接口。所有读写操作都通过内存映射方式进行。
 */
    class BRAMMapper {
    public:
        /**
         * @brief 构造函数，初始化物理地址和映射大小。
         *
         * @param physicalAddress 物理地址。
         * @param mapSize 映射大小。
         */
        BRAMMapper(off_t physicalAddress, size_t mapSize)
                : physicalAddress_(alignToPage(physicalAddress)),
                  mapSize_(adjustSizeForAlignment(physicalAddress, mapSize)),
                  mappedBase_(nullptr),
                  fd_(-1) {
            mapMemory();
        }

        /**
         * @brief 析构函数，解除内存映射。
         */
        ~BRAMMapper() {
            unmapMemory();
        }

        /**
         * @brief 读取指定偏移的值（线程安全）。
         *
         * @tparam T 数据类型（如 uint32_t, uint64_t 等）。
         * @param offset 读取的偏移位置。
         * @return 读取的值。
         *
         * @throws std::out_of_range 如果偏移超出映射区域。
         */
        template<typename T>
        T read(size_t offset) {
            std::lock_guard<std::mutex> lock(mutex_);
            validateOffset<T>(offset);
            return *reinterpret_cast<volatile T *>(static_cast<uint8_t *>(mappedBase_) + offset);
        }

        /**
         * @brief 向指定偏移写入值（线程安全）。
         *
         * @tparam T 数据类型（如 uint32_t, uint64_t 等）。
         * @param offset 写入的偏移位置。
         * @param value 写入的值。
         *
         * @throws std::out_of_range 如果偏移超出映射区域。
         */
        template<typename T>
        void write(size_t offset, T value) {
            std::lock_guard<std::mutex> lock(mutex_);
            validateOffset<T>(offset);
            *reinterpret_cast<volatile T *>(static_cast<uint8_t *>(mappedBase_) + offset) = value;
        }

    private:
        off_t physicalAddress_; // 物理地址
        size_t mapSize_; // 映射大小
        void *mappedBase_; // 映射的基地址
        int fd_; // 文件描述符
        std::mutex mutex_; // 线程安全互斥锁

        /**
         * @brief 将物理地址映射到页面边界。
         *
         * @param address 物理地址。
         * @return 对齐后的物理地址。
         */
        static off_t alignToPage(off_t address) {
            long pageSize = sysconf(_SC_PAGESIZE);
            if (pageSize <= 0) {
                throw std::runtime_error("Failed to get page size");
            }
            return address & ~(pageSize - 1);
        }

        /**
         * @brief 调整映射大小以适应内存对齐要求。
         *
         * @param address 物理地址。
         * @param size 映射的原始大小。
         * @return 调整后的映射大小。
         */
        static size_t adjustSizeForAlignment(off_t address, size_t size) {
            long pageSize = sysconf(_SC_PAGESIZE);
            if (pageSize <= 0) {
                throw std::runtime_error("Failed to get page size");
            }
            size_t alignmentOffset = address & (pageSize - 1);
            return size + alignmentOffset;
        }

        /**
         * @brief 映射内存区域。
         *
         * 打开/dev/mem文件并通过mmap将指定物理地址区域映射到用户空间。
         *
         * @throws std::runtime_error 如果打开或映射失败。
         */
        void mapMemory() {
            fd_ = open("/dev/mem", O_RDWR | O_SYNC);
            if (fd_ < 0) {
                throw std::runtime_error("Failed to open /dev/mem");
            }

            mappedBase_ = mmap(nullptr, mapSize_, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, physicalAddress_);
            if (mappedBase_ == MAP_FAILED) {
                close(fd_);
                throw std::runtime_error("Memory mapping failed");
            }
        }

        /**
         * @brief 解除内存映射。
         */
        void unmapMemory() {
            if (mappedBase_ && mappedBase_ != MAP_FAILED) {
                munmap(mappedBase_, mapSize_);
            }
            if (fd_ >= 0) {
                close(fd_);
            }
        }

        /**
         * @brief 验证偏移是否超出映射区域。
         *
         * @tparam T 数据类型（如 uint32_t, uint64_t 等）。
         * @param offset 偏移位置。
         *
         * @throws std::out_of_range 如果偏移超出映射区域。
         */
        template<typename T>
        void validateOffset(size_t offset) const {
            if (offset + sizeof(T) > mapSize_) {
                throw std::out_of_range("Offset out of range");
            }
        }
    };
}

#endif // BRAM_MAPPER_H
