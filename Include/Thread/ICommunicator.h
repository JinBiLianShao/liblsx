/**
 * @file ICommunicator.h
 * @brief 线程间通信接口定义
 * @details 定义了 LSX_LIB::Thread 命名空间下的抽象基类 ICommunicator，
 * 这是一个占位接口，用于未来集成线程间消息传递功能。
 * 它提供了一套标准的接口，允许不同的线程通过发送消息和注册处理函数的方式进行异步通信。
 * 具体实现将负责底层的线程安全消息队列、事件通知或信号机制。
 * @author 连思鑫（liansixin）
 * @date 2025-5-7
 * @version 1.0 (占位接口版本)
 *
 * ### 核心功能
 * - **接口标准化**: 定义了线程间消息发送和处理的标准接口。
 * - **异步通信**: 支持通过消息队列或类似机制进行非阻塞的线程间通信。
 * - **回调机制**: 允许线程注册回调函数来处理接收到的消息。
 * - **多态支持**: 作为抽象基类，允许通过基类指针操作不同的具体通信实现。
 *
 * ### 使用示例
 *
 * @code
 * #include "ICommunicator.h"
 * #include <iostream>
 * #include <thread>
 * #include <vector>
 * #include <queue>
 * #include <mutex>
 * #include <condition_variable>
 *
 * // 示例：一个简单的基于队列的 ICommunicator 实现 (仅为演示概念)
 * class SimpleQueueCommunicator : public LSX_LIB::Thread::ICommunicator {
 * public:
 * void postMessage(const std::string& msg) override {
 * std::lock_guard<std::mutex> lock(queue_mutex_);
 * message_queue_.push(msg);
 * cv_.notify_one(); // 通知等待接收的线程
 * }
 *
 * void registerHandler(std::function<void(const std::string&)> handler) override {
 * // 在实际应用中，handler 的注册和管理需要更严谨的线程安全处理
 * // 例如，使用原子操作或额外的锁保护 handler_
 * handler_ = handler;
 * }
 *
 * // 示例：一个简单的接收循环 (通常在接收线程中运行)
 * void run_receive_loop() {
 * while (true) { // 实际应用中需要优雅退出机制
 * std::string msg;
 * {
 * std::unique_lock<std::mutex> lock(queue_mutex_);
 * cv_.wait(lock, [&]{ return !message_queue_.empty(); }); // 等待消息
 * msg = message_queue_.front();
 * message_queue_.pop();
 * }
 * if (handler_) {
 * handler_(msg); // 调用注册的处理函数
 * }
 * }
 * }
 *
 * private:
 * std::queue<std::string> message_queue_;
 * std::mutex queue_mutex_;
 * std::condition_variable cv_;
 * std::function<void(const std::string&)> handler_; // 消息处理函数
 * };
 *
 * // 示例消息处理函数
 * void handle_received_message(const std::string& msg) {
 * std::cout << "[Thread " << std::this_thread::get_id() << "] Received message: " << msg << std::endl;
 * }
 *
 * int main() {
 * // 创建一个通信器实例
 * auto communicator = std::make_unique<SimpleQueueCommunicator>();
 *
 * // 注册消息处理函数
 * communicator->registerHandler(handle_received_message);
 *
 * // 启动一个接收线程 (模拟)
 * std::thread receiver_thread(&SimpleQueueCommunicator::run_receive_loop, communicator.get());
 *
 * // 在主线程发送消息
 * std::cout << "[Main Thread] Posting message 1..." << std::endl;
 * communicator->postMessage("Hello from main thread!");
 *
 * std::this_thread::sleep_for(std::chrono::milliseconds(100)); // 等待消息处理
 *
 * std::cout << "[Main Thread] Posting message 2..." << std::endl;
 * communicator->postMessage("Another message!");
 *
 * std::this_thread::sleep_for(std::chrono::milliseconds(100)); // 等待消息处理
 *
 * // 实际应用中需要机制来停止接收线程和清理
 * // receiver_thread.join(); // 示例中 run_receive_loop 是无限循环，这里会阻塞
 *
 * std::cout << "主线程退出。" << std::endl;
 * return 0;
 * }
 * @endcode
 *
 * ### 注意事项
 * - **抽象接口**: ICommunicator 是一个纯虚类，不能直接实例化，需要具体的派生类来实现其方法。
 * - **具体实现**: 实际的线程间通信机制（如消息队列、管道、共享内存+信号量等）需要在派生类中实现。
 * - **线程安全**: 具体的实现类必须保证 `postMessage` 和 `registerHandler` 方法的线程安全，以及消息队列/机制的线程安全。
 * - **消息处理**: `registerHandler` 注册的回调函数将在哪个线程中执行取决于具体实现。常见的模式是在专门的接收线程中处理消息。
 * - **生命周期**: 确保 ICommunicator 对象的生命周期覆盖所有需要发送和接收消息的线程。
 */

//
// Created by admin on 2025/5/7.
//

#ifndef ICOMMUNICATOR_H
#define ICOMMUNICATOR_H
#pragma once

#include <functional> // 包含 std::function
#include <string> // 包含 std::string
#include <memory> // 包含 std::unique_ptr 等智能指针

/**
 * @brief LSX 库的根命名空间。
 */
namespace LSX_LIB
{
    /**
     * @brief 线程相关的命名空间。
     * 包含线程管理和线程间通信相关的类和工具。
     */
    namespace Thread
    {
        /**
         * @brief 线程间通信接口定义 (占位)。
         * 这是一个抽象基类，定义了线程间进行异步消息传递的标准接口。
         * 具体的实现类将提供实际的消息发送和接收机制。
         * 用于未来集成线程间消息传递功能。
         */
        class ICommunicator
        {
        public:
            /**
             * @brief 虚析构函数。
             * 确保通过基类指针删除派生类对象时，能够正确调用派生类的析构函数，释放所有资源。
             */
            virtual ~ICommunicator() = default;

            /**
             * @brief 发送消息给其他线程。
             * 将指定的消息内容放入通信机制中，供其他线程接收。
             * 接收消息的线程将通过注册的处理回调函数来接收和处理消息。
             * 此方法通常是异步且非阻塞的。
             *
             * @param msg 要发送的消息内容字符串。
             */
            virtual void postMessage(const std::string& msg) = 0;

            /**
             * @brief 注册消息处理回调函数。
             * 设置一个函数，当接收到消息时将被调用。
             * 通常，一个 ICommunicator 实例只支持注册一个消息处理函数。
             * 如果需要多个处理函数，具体的实现类可能需要内部管理一个处理函数列表。
             *
             * @param handler 一个函数对象，接受一个 const std::string& 参数，表示接收到的消息内容。
             */
            virtual void registerHandler(std::function<void(const std::string&)> handler) = 0;
        };
    } // namespace Thread
} // namespace LSX_LIB
#endif //ICOMMUNICATOR_H
