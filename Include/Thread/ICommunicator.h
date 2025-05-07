//
// Created by admin on 2025/5/7.
//

#ifndef ICOMMUNICATOR_H
#define ICOMMUNICATOR_H
#pragma once

#include <functional>
#include <string>
#include <memory>

namespace LSX_LIB::Thread
{
    /**
     * @brief 线程间通信接口定义 (占位)
     * 用于未来集成线程间消息传递功能
     */
    class ICommunicator
    {
    public:
        virtual ~ICommunicator() = default;

        /**
         * @brief 发送消息给其他线程 (通过注册的handler接收)
         * @param msg 要发送的消息内容
         */
        virtual void postMessage(const std::string& msg) = 0;

        /**
         * @brief 注册消息处理回调函数
         * @param handler 接收到消息时调用的函数
         */
        virtual void registerHandler(std::function<void(const std::string&)> handler) = 0;
    };
}
#endif //ICOMMUNICATOR_H
