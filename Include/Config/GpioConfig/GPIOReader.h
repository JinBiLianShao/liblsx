/*********************************************************************
 * Created by user on 2023/7/31.
 * 作者 : 连思鑫
 * 文件名 : GPIOReader.h
 * 功能 : 用于读写GPIO的状态，支持读取和写入操作。
 *
 * 类 : GPIOReader
 * 方法 :
 *    - GPIOReader(const std::string &gpioPath)
 *      初始化GPIOReader对象，绑定指定的GPIO路径。
 *
 *    - bool readGPIOState()
 *      读取GPIO的当前状态，返回高/低电平。
 *
 *    - bool WriteGPIOState(bool state)
 *      写入GPIO的状态，设置为高/低电平。
 *
 *    - bool hanError() const
 *      检查是否存在初始化错误。
 *
 * 使用示例 :
 * ```cpp
 * #include "GPIOReader.h"
 *
 * int main() {
 *     GPIOReader gpioReader("/sys/class/gpio/gpio27");
 *     if (!gpioReader.hanError()) {
 *         bool gpioState = gpioReader.readGPIOState();
 *         std::cout << "GPIO State: " << (gpioState ? "High" : "Low") << std::endl;
 *         gpioReader.WriteGPIOState(!gpioState);
 *     } else {
 *         std::cerr << "Failed to initialize GPIOReader." << std::endl;
 *     }
 *     return 0;
 * }
 * ```
 * 注意 : 在使用 `GPIOReader` 前，请确保提供的GPIO路径有效，并且具有访问权限。
 *********************************************************************/

#ifndef KKTRAFFIC_GPIOREADER_H
#define KKTRAFFIC_GPIOREADER_H

#include <iostream>
#include <fstream>
#include <string>
namespace LSX_LIB::Config {
    class GPIOReader {
    public:
        /*********************
         * 功能 : 初始化GPIOReader对象并绑定GPIO路径。
         * 参数 : const std::string &gpioPath - GPIO路径。
         * 返回值 : 无。
         * 注意 : 如果路径无效，`hanError()` 会返回 true。
         *********************/
        GPIOReader(const std::string &gpioPath)
                : gpioPath_(gpioPath),
                  valueFilePath_(gpioPath + "/value"),
                  hasError_(false) {
            if (!checkFileExistence(valueFilePath_)) {
                std::cerr << "GPIO value file does not exist." << std::endl;
                hasError_ = true;
            }
        }

        /*********************
         * 功能 : 读取GPIO的当前状态。
         * 返回值 :
         *    - true : 高电平。
         *    - false : 低电平或读取失败。
         * 注意 : 如果初始化失败，直接返回 false。
         *********************/
        bool readGPIOState() {
            if (hasError_) {
                return false;
            }
            std::ifstream valueFile(valueFilePath_);
            if (valueFile.is_open()) {
                std::string value;
                valueFile >> value;
                valueFile.close();
                return (value == "1");
            } else {
                std::cerr << "Failed to open GPIO value file" << std::endl;
                return false;
            }
        }

        /*********************
         * 功能 : 写入GPIO状态。
         * 参数 : bool state - true 表示高电平，false 表示低电平。
         * 返回值 :
         *    - true : 写入成功。
         *    - false : 写入失败。
         * 注意 : 如果初始化失败，直接返回 false。
         *********************/
        bool WriteGPIOState(bool state) {
            if (hasError_) {
                return false;
            }
            std::ofstream valueFile(valueFilePath_);
            if (valueFile.is_open()) {
                valueFile << (state ? "1" : "0");
                valueFile.close();
                return true;
            } else {
                std::cerr << "Failed to open GPIO value file for writing" << std::endl;
                return false;
            }
        }

        /*********************
         * 功能 : 检查初始化是否有错误。
         * 返回值 :
         *    - true : 存在错误。
         *    - false : 无错误。
         *********************/
        bool hanError() const {
            return hasError_;
        }

    private:
        /*********************
         * 功能 : 检查文件是否存在。
         * 参数 : const std::string &filePath - 文件路径。
         * 返回值 :
         *    - true : 文件存在。
         *    - false : 文件不存在。
         *********************/
        bool checkFileExistence(const std::string &filePath) {
            std::ifstream file(filePath);
            return file.good();
        }

        std::string gpioPath_; // GPIO路径。
        std::string valueFilePath_; // GPIO值文件路径。
        bool hasError_; // 标志是否有错误。
    };
}
#endif // KKTRAFFIC_GPIOREADER_H
