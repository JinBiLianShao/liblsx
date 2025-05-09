/**
* @file LSX_Memory.h
 * @brief 数据传输工具库 - 内存模块主头文件
 * @details 这是一个便利头文件，用于包含 LSX_LIB::Memory 命名空间下所有内存模块的头文件。
 * 包含此头文件即可访问缓冲区、各种队列（FIFO、循环、固定大小）和管道、以及共享内存等功能。
 * @author 连思鑫（liansixin）
 * @date 2025-5-7
 * @version 1.0
 *
 * ### 包含模块
 * - Buffer: 通用内存缓冲区
 * - CircularFixedSizeQueue: 循环固定大小内存块队列
 * - CircularQueue: 循环队列 (模板)
 * - FIFO: 先进先出队列 (模板)
 * - FixedSizePipe: 固定大小内存块管道
 * - FixedSizeQueue: 固定大小内存块队列
 * - Pipe: 管道 (模板)
 * - Queue: 队列 (模板)
 * - SharedMemory: 共享内存
 *
 * ### 使用示例
 *
 * @code
 * #include "LSX_Memory.h"
 * #include <iostream>
 *
 * int main() {
 * // 使用 Buffer 类
 * LIB_LSX::Memory::Buffer buffer(100);
 * std::cout << "Buffer size: " << buffer.GetSize() << std::endl;
 *
 * // 使用 FIFO 队列
 * LIB_LSX::Memory::FIFO<int> int_fifo;
 * int_fifo.Put(10);
 * std::cout << "FIFO size: " << int_fifo.Size() << std::endl;
 *
 * // 使用 FixedSizeQueue
 * LIB_LSX::Memory::FixedSizeQueue byte_queue(10, 5);
 * std::vector<uint8_t> data = {1, 2, 3};
 * if (byte_queue.Put(data)) {
 * std::cout << "FixedSizeQueue size: " << byte_queue.Size() << std::endl;
 * }
 *
 * // ... 可以使用包含的所有其他内存模块类 ...
 *
 * std::cout << "主线程退出。" << std::endl;
 * return 0;
 * }
 * @endcode
 *
 * ### 注意事项
 * - 包含此头文件会引入所有内存模块的依赖。如果只需要其中部分功能，建议直接包含所需的具体头文件。
 */

#ifndef LIB_LSX_MEMORY_LSX_MEMORY_H
#define LIB_LSX_MEMORY_LSX_MEMORY_H
#pragma once
// 包含全局错误锁头文件
#include "GlobalErrorMutex.h"
// 包含 LIBLSX::LockManager::LockGuard 头文件
#include "LockGuard.h"
// 主头文件，包含所有内存模块的头文件

#include "Buffer.h" // 通用内存缓冲区
#include "CircularFixedSizeQueue.h" // 循环固定大小内存块队列
#include "CircularQueue.h" // 循环队列 (模板)
#include "FIFO.h" // 先进先出队列 (模板)
#include "FixedSizePipe.h" // 固定大小内存块管道
#include "FixedSizeQueue.h" // 固定大小内存块队列
#include "Pipe.h" // 管道 (模板)
#include "Queue.h" // 队列 (模板)
#include "SharedMemory.h" // 共享内存


#endif // LIB_LSX_MEMORY_LSX_MEMORY_H
