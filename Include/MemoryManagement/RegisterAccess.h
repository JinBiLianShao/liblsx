/**********************************
 * @file RegisterAccess.h
 * @author 连思鑫
 * @date 10/11/2023
 * @version 1.0
 * @brief 用于通过devmem工具读写寄存器地址的类。
 *
 * @details
 * 该类封装了通过devmem工具读写寄存器地址的功能，提供了简单的接口来读取和写入寄存器的值。
 *
 * @example
 * 以下是一个使用示例：
 * @code
 * int main() {
 *     RegisterAccess regAccess(0x4000000C); // 初始化寄存器地址
 *     int value = regAccess.readRegister(); // 读取寄存器的值
 *     if (value == static_cast<unsigned int>(-1)) return -1;
 *     std::cout << "Original Value: " << std::hex << value << std::endl;
 *     int inverted_value = ~value; // 对值取反
 *     std::cout << "Inverted Value: " << std::hex << inverted_value << std::endl;
 *     regAccess.writeRegister(inverted_value); // 将取反后的值写入寄存器
 *     return 0;
 * }
 * @endcode
 *************************************/

#ifndef KKTRAFFIC_REGISTERACCESS_H
#define KKTRAFFIC_REGISTERACCESS_H

#include <iostream>
#include <cstdio>
#include <string>
#include <stdexcept>
/**
 * @brief LSX 库的根命名空间。
 */
namespace LIB_LSX::Memory {
/**
 * @class RegisterAccess
 * @brief 用于读写寄存器地址的类。
 *
 * @details
 * 该类通过调用devmem工具实现对寄存器地址的读写操作。
 */
    class RegisterAccess {
    private:
        std::string devmem_command; ///< 用于存储devmem命令的字符串

    public:
        /**
         * @brief 构造函数，初始化寄存器地址。
         *
         * @param phys_addr 要操作的寄存器物理地址。
         */
        RegisterAccess(unsigned long phys_addr) : devmem_command("devmem " + std::to_string(phys_addr)) {
        }

        /**
         * @brief 读取寄存器的值。
         *
         * @return unsigned int 读取到的寄存器值。
         *
         * @throws std::runtime_error 如果执行命令失败或读取值失败。
         *
         * @example
         * @code
         * RegisterAccess regAccess(0x4000000C);
         * int value = regAccess.readRegister();
         * std::cout << "Value: " << std::hex << value << std::endl;
         * @endcode
         */
        unsigned int readRegister() {
            FILE *pipe = popen(devmem_command.c_str(), "r");

            if (!pipe) {
                perror("Error executing command");
                throw std::runtime_error("Error executing command");
            }

            unsigned int value;
            if (fscanf(pipe, "0x%x", &value) != 1) {
                std::cerr << "Error reading value\n";
                pclose(pipe);
                throw std::runtime_error("Error reading value");
            }

            pclose(pipe);

            return value;
        }

        /**
         * @brief 将值写入寄存器。
         *
         * @param value 要写入寄存器的值。
         *
         * @throws std::runtime_error 如果执行命令失败。
         *
         * @example
         * @code
         * RegisterAccess regAccess(0x4000000C);
         * regAccess.writeRegister(0x1234);
         * @endcode
         */
        void writeRegister(unsigned int value) {
            std::string write_command = devmem_command + " 32 " + std::to_string(value);
            int result = system(write_command.c_str());
            if (result == -1) {
                perror("Error executing command");
                throw std::runtime_error("Error executing command");
            }
        }
    };
}

#endif //KKTRAFFIC_REGISTERACCESS_H
