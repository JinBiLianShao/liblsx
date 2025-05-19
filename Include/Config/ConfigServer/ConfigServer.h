/**
 * @file ConfigServer.h
 * @brief HTTP 服务器封装类，提供配置项的 CRUD (创建、读取、更新、删除) 操作接口。
 * @author 连思鑫
 */

#ifndef CONFIG_SERVER_H // 防止头文件重复包含
#define CONFIG_SERVER_H

#include <iostream>      // 用于标准输入输出 (例如 cout, cerr)
#include <string>        // 用于字符串操作
#include <vector>        // 用于存储查询结果等
#include <functional>    // 用于 std::function (回调处理器)
#include <stdexcept>     // 用于标准异常 (例如 runtime_error, out_of_range, invalid_argument)

#include <httplib.h>               // HTTP库，用于创建HTTP服务器 (cpp-httplib)
#include <nlohmann/json.hpp>       // JSON库，用于处理JSON数据 (nlohmann/json)
#include "SQLiteDB.h"              // SQLite数据库操作类 (您提供的头文件)

// 使用 httplib 和 nlohmann/json 的命名空间简化代码
using namespace httplib;
using json = nlohmann::json;

/**
 * @brief LSX_LIB 库的根命名空间
 */
namespace LSX_LIB {
    /**
     * @brief Config 模块的命名空间，用于配置管理相关功能。
     */
    namespace Config {
        /**
         * @class ConfigServer
         * @brief 基于 HTTP 的配置项管理服务器。
         *
         * 此类封装了一个简单的 HTTP 服务器，通过 API 接口提供对 SQLite 数据库中
         * 配置项的 CRUD (创建、读取、更新、删除) 操作。
         * 服务器直接内嵌 HTML 页面作为用户操作界面。
         *
         * @example 基本使用示例
         * @code
         * int main() {
         * // 创建服务器实例，指定数据库文件路径和监听端口
         * LSX_LIB::Config::ConfigServer server("data.db", ".", 3000);
         *
         * // 初始化数据库表和API路由
         * server.initialize();
         *
         * // 启动服务器
         * server.run();
         *
         * return 0;
         * }
         * @endcode
         */
        class ConfigServer {
        public:
            /**
             * @brief ConfigServer 构造函数。
             * @param dbPath SQLite 数据库文件的路径。
             * @param baseDir 静态文件服务的基础目录 (默认为当前目录 ".")。
             * 由于首页已内嵌，此目录主要用于提供其他外部静态资源 (如独立的CSS/JS文件，图片等)。
             * @param port 服务器监听的端口号 (默认为 3000)。
             */
            ConfigServer(const std::string &dbPath, const std::string &baseDir = ".", int port = 3000)
                    : db_(dbPath), port_(port), baseDir_(baseDir) { // db_ 使用您提供的 LSX_LIB::SQL::SQLiteDB
                // 构造函数体，如有需要可添加初始化逻辑
            }

            /**
             * @brief 初始化服务器。
             *
             * 该方法执行以下操作：
             * 1. 初始化数据库表结构 (如果表不存在则创建)。
             * 2. 设置处理 HTTP 请求的默认路由 (API接口和首页)。
             * 3. 如果指定了有效的 baseDir，则配置静态文件服务。
             */
            void initialize() {
                initializeDatabase(); // 初始化数据库
                setupRoutes();        // 设置路由
                // 如果 baseDir 不是当前目录 "." 且不为空，则启用静态文件服务
                if (baseDir_ != "." && !baseDir_.empty()) {
                    serveStaticFiles();
                }
            }

            /**
             * @brief 启动 HTTP 服务器。
             *
             * 服务器将开始在指定的 IP 地址 (0.0.0.0，表示所有可用网络接口) 和端口上监听传入的 HTTP 请求。
             */
            void run() {
                std::cout << "服务器正在运行于 http://localhost:" << port_ << std::endl;
                svr_.listen("0.0.0.0", port_); // 监听所有网络接口
            }

            /**
             * @brief 添加自定义 HTTP 路由。
             *
             * 允许用户在默认路由之外添加自定义的请求处理器。
             * @param method HTTP 方法 (例如 "GET", "POST", "PUT", "DELETE")。
             * @param pattern URL 路由模式 (例如 "/custom/path", "/items/:id")。
             * @param handler 处理该路由请求的回调函数 (std::function<void(const Request &, Response &)>)。
             *
             * @example
             * @code
             * server.addCustomRoute("GET", "/status", [](const Request& req, Response& res) {
             * res.set_content("服务器状态：正常", "text/plain; charset=utf-8");
             * });
             * @endcode
             */
            void addCustomRoute(const std::string &method, const std::string &pattern,
                                const std::function<void(const Request &, Response &)>& handler) {
                if (method == "GET") {
                    svr_.Get(pattern.c_str(), handler);
                } else if (method == "POST") {
                    svr_.Post(pattern.c_str(), handler);
                } else if (method == "PUT") {
                    svr_.Put(pattern.c_str(), handler);
                } else if (method == "DELETE") {
                    svr_.Delete(pattern.c_str(), handler);
                } else {
                    std::cerr << "错误：不支持的 HTTP 方法: " << method << std::endl;
                    throw std::runtime_error("不支持的 HTTP 方法: " + method);
                }
            }

        private:
            /**
             * @brief 初始化数据库表结构。
             *
             * 如果名为 "config" 的表不存在，则创建该表。
             * 表包含 "id" (主键,自增), "key" (文本,非空,唯一), "value" (文本,非空) 三个字段。
             */
            void initializeDatabase() {
                // SQLiteDB::createTable 内部会检查表是否存在，若存在会打印消息并返回。
                // 此处的 tableExists 调用是为了更明确地在 ConfigServer层面记录日志或行为。
                // 如果不希望双重检查，可以直接调用 db_.createTable。
                if (!db_.tableExists("config")) {
                    std::cout << "正在创建表 'config'..." << std::endl;
                    db_.createTable("config", {
                            "id INTEGER PRIMARY KEY AUTOINCREMENT",
                            "key TEXT NOT NULL UNIQUE", // 配置键，必须唯一且不为空
                            "value TEXT NOT NULL"       // 配置值，不为空
                    });
                } else {
                    std::cout << "表 'config' 已存在。" << std::endl;
                }
            }

            /**
             * @brief 设置服务器的默认路由规则。
             *
             * 包括：
             * - 根路径 ("/")：提供内嵌的 HTML 管理界面。
             * - API 路径 ("/api/data")：用于配置项的 CRUD 操作。
             */
            void setupRoutes() {
                // 根路径 ("/") 的 GET 请求，提供管理界面的 HTML 页面
                svr_.Get("/", [this](const Request &req, Response &res) {
                    serveIndexPage(res);
                });

                // API: 获取所有配置项
                svr_.Get("/api/data", [this](const Request &req, Response &res) {
                    getAllConfigs(req, res);
                });

                // API: 新增配置项
                svr_.Post("/api/data", [this](const Request &req, Response &res) {
                    addConfig(req, res);
                });

                // API: 更新指定ID的配置项
                // :id 是路径参数，表示配置项的ID
                svr_.Put("/api/data/:id", [this](const Request &req, Response &res) {
                    updateConfig(req, res);
                });

                // API: 删除指定ID的配置项
                svr_.Delete("/api/data/:id", [this](const Request &req, Response &res) {
                    deleteConfig(req, res);
                });
            }

            /**
             * @brief 配置静态文件服务。
             *
             * 如果构造函数中指定了有效的 `baseDir_`，则设置该目录为静态文件根目录。
             * 这允许服务器提供如 CSS、JavaScript、图片等外部静态文件。
             */
            void serveStaticFiles() {
                // 确保 baseDir_ 不为空且不是当前目录 "." (因为首页已内嵌)
                if (!baseDir_.empty() && baseDir_ != ".") {
                    // httplib 的 set_base_dir 用于设置静态文件服务的根目录
                    if (svr_.set_base_dir(baseDir_.c_str())) {
                        std::cout << "静态文件服务目录设置为: " << baseDir_ << std::endl;
                    } else {
                        // 如果设置失败 (例如目录不存在或无权限)，打印警告信息
                        std::cerr << "警告：设置静态文件服务目录失败: " << baseDir_
                                  << "。请检查目录是否存在且可访问。" << std::endl;
                    }
                }
            }

            /**
             * @brief 提供内嵌的首页 HTML 页面。
             *
             * HTML 内容作为C++原始字符串字面量直接嵌入到代码中，
             * 避免了对外部 index.html 文件的依赖。
             * @param res HTTP 响应对象，用于设置响应内容和类型。
             */
            void serveIndexPage(Response &res) {
                // 使用C++原始字符串字面量 (raw string literal) R"DELIMITER(...)DELIMITER" 嵌入HTML内容。
                // 这样可以避免手动转义HTML中的特殊字符 (如引号、换行符等)。
                // DELIMITER 可以是任何不出现在HTML内容中的字符序列。
                std::string html_content = R"HTML_CONTENT(
<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>动态配置 - CRUD</title>
    <style>
        /* CSS样式开始 */
        body {
            font-family: Arial, sans-serif; /* 字体 */
            margin: 20px;                   /* 外边距 */
            background-color: #f8f9fa;      /* 背景色 */
            color: #333;                    /* 文字颜色 */
        }
        #app { /* 主应用容器 */
            background-color: white;        /* 背景色 */
            padding: 20px;                  /* 内边距 */
            border-radius: 8px;             /* 圆角 */
            box-shadow: 0 2px 10px rgba(0, 0, 0, 0.1); /* 阴影效果 */
            max-width: 800px;               /* 最大宽度，优化宽屏显示 */
            margin-left: auto;              /* 水平居中 */
            margin-right: auto;
        }
        table { /* 表格样式 */
            border-collapse: collapse;      /* 边框合并 */
            width: 100%;                    /* 宽度100% */
            margin-top: 20px;               /* 上边距 */
        }
        th, td { /* 表头和单元格样式 */
            border: 1px solid #dee2e6;      /* 边框 */
            padding: 12px;                  /* 内边距 */
            text-align: left;               /* 左对齐 */
            word-break: break-all;          /* 长单词或URL换行，防止破坏布局 */
        }
        th { /* 表头单元格特定样式 */
            background-color: #e9ecef;      /* 背景色 */
            font-weight: bold;              /* 字体加粗 */
        }
        tr:nth-child(even) { /* 表格偶数行样式 */
            background-color: #f8f9fa;
        }
        tr:hover { /* 表格行鼠标悬停样式 */
            background-color: #e2e6ea;
        }
        .form-modal { /* 表单模态框样式 */
            display: none;                  /* 默认隐藏 */
            position: fixed;                /* 固定定位 */
            top: 50%;                       /* 垂直居中 */
            left: 50%;                      /* 水平居中 */
            transform: translate(-50%, -50%); /* 精确居中 */
            background: white;
            padding: 30px;
            box-shadow: 0 5px 15px rgba(0, 0, 0, 0.2);
            z-index: 1000;                  /* 置于顶层 */
            border-radius: 8px;
            width: 90%;                     /* 宽度，响应式 */
            max-width: 500px;               /* 最大宽度 */
        }
        .form-modal.active { /* 激活状态的模态框 */
            display: block;                 /* 显示 */
        }
        .overlay { /* 遮罩层样式 */
            display: none;                  /* 默认隐藏 */
            position: fixed;
            top: 0;
            left: 0;
            width: 100%;
            height: 100%;
            background: rgba(0, 0, 0, 0.6); /* 半透明黑色背景 */
            z-index: 999;                   /* 在模态框之下 */
        }
        .overlay.active { /* 激活状态的遮罩层 */
            display: block;
        }
        button { /* 通用按钮样式 */
            padding: 10px 15px;
            cursor: pointer;                /* 鼠标指针样式 */
            border: none;                   /* 无边框 */
            border-radius: 4px;
            font-size: 14px;
            margin-right: 10px;             /* 右边距 */
            transition: background-color 0.2s ease-in-out; /* 背景色过渡动画 */
        }
        button:last-child {
            margin-right: 0; /* 最后一个按钮无右边距 */
        }
        button[type="submit"], #app > button { /* 主要操作按钮 (如提交、新增配置) */
            background-color: #007bff;      /* 主题蓝色 */
            color: white;
        }
        button[type="submit"]:hover, #app > button:hover {
            background-color: #0056b3;      /* 鼠标悬停颜色加深 */
        }
        button[type="button"] { /* 次要/取消按钮 */
             background-color: #6c757d;     /* 灰色 */
             color: white;
        }
        button[type="button"]:hover {
            background-color: #5a6268;
        }
        #app > button { /* "新增配置"按钮特定样式 */
            margin-bottom: 20px;
        }
        td button { /* 表格内操作按钮样式 */
            padding: 6px 10px;
            font-size: 13px;
            margin-bottom: 5px;             /* 同一单元格内按钮的下边距 */
        }
        .button-edit { /* 编辑按钮特定样式 */
            background-color: #ffc107;      /* 警告黄色 */
            color: #212529;                 /* 深色文字 */
        }
        .button-edit:hover {
            background-color: #e0a800;
        }
        .button-delete { /* 删除按钮特定样式 */
            background-color: #dc3545;      /* 危险红色 */
            color: white;
        }
        .button-delete:hover {
            background-color: #c82333;
        }
        #dataForm div { /* 表单内字段容器样式 */
            margin-bottom: 15px;
        }
        #dataForm label { /* 表单标签样式 */
            display: block;
            margin-bottom: 5px;
            font-weight: bold;
        }
        #dataForm input[type="text"] { /* 表单文本输入框样式 */
            width: calc(100% - 22px);       /* 考虑内边距和边框后的宽度 */
            padding: 10px;
            border: 1px solid #ced4da;
            border-radius: 4px;
            font-size: 14px;
        }
        #dataForm input[disabled] { /* 禁用的输入框样式 */
            background-color: #e9ecef;
            cursor: not-allowed;            /* 禁用鼠标指针 */
        }
        #formTitle { /* 模态框标题样式 */
            margin-top: 0;
            margin-bottom: 20px;
            font-size: 20px;
            color: #333;
            border-bottom: 1px solid #eee; /* 标题下划线 */
            padding-bottom: 10px;
        }
        .error-message { /* API错误信息显示区域样式 */
            color: red;
            margin-top: 10px;
            margin-bottom: 10px; /* 与表格保持一定间距 */
            padding: 10px;
            border: 1px solid red;
            background-color: #ffebeb; /* 淡红色背景 */
            border-radius: 4px;
            display: none;              /* 默认隐藏 */
        }
        /* CSS样式结束 */
    </style>
</head>
<body>
<div id="app"> <h1>动态配置管理</h1>
    <button onclick="showAddForm()">新增配置</button>
    <div id="apiError" class="error-message"></div> <table id="dataTable"> <thead>
            </thead>
        <tbody>
            </tbody>
    </table>

    <div class="overlay" id="overlay" onclick="hideForm()"></div> <div class="form-modal" id="formModal">
        <h3 id="formTitle">新增配置</h3> <form id="dataForm" onsubmit="event.preventDefault(); submitForm();"> <div id="formFields">
                </div>
            <button type="submit">提交</button>
            <button type="button" onclick="hideForm()">取消</button>
        </form>
    </div>
</div>

<script>
    // JavaScript逻辑开始
    const columns = ['id', 'key', 'value']; // 定义表格列名 (与后端数据字段对应)
    let currentData = []; // 用于存储从API获取的当前配置数据
    let isEditMode = false; // 标记当前是否为编辑模式
    let editId = null; // 存储当前正在编辑的配置项的ID

    const apiErrorDiv = document.getElementById('apiError'); // 获取API错误显示区域的DOM元素

    /**
     * @brief 显示API相关的错误信息。
     * @param {string} message 要显示的错误消息。
     */
    function displayApiError(message) {
        apiErrorDiv.textContent = message;
        apiErrorDiv.style.display = 'block'; // 显示错误区域
    }

    /**
     * @brief 清除API错误信息。
     */
    function clearApiError() {
        apiErrorDiv.textContent = '';
        apiErrorDiv.style.display = 'none'; // 隐藏错误区域
    }

    // 页面加载完成后自动执行的函数
    window.onload = async () => {
        await fetchData(); // 获取并渲染初始数据
    };

    /**
     * @brief 从API获取配置数据。
     * 获取成功后会调用 renderTable 渲染表格。
     * 获取失败则显示错误信息。
     */
    async function fetchData() {
        clearApiError(); // 开始获取数据前清除旧的错误信息
        try {
            const res = await fetch('/api/data'); // 调用后端API
            if (!res.ok) { // 检查HTTP响应状态码
                // 尝试解析JSON格式的错误信息，如果失败则使用HTTP状态文本
                const errorData = await res.json().catch(() => ({ message: `HTTP错误！状态码: ${res.status}` }));
                throw new Error(errorData.message || `HTTP错误！状态码: ${res.status}`);
            }
            currentData = await res.json(); // 解析JSON响应体
            renderTable(); // 渲染表格
        } catch (error) {
            console.error("获取数据失败:", error);
            displayApiError(`获取配置数据失败: ${error.message}`);
        }
    }

    /**
     * @brief 渲染HTML表格。
     * 根据 currentData 数组的内容动态生成表格的表头和数据行。
     */
    function renderTable() {
        const thead = document.querySelector('#dataTable thead');
        const tbody = document.querySelector('#dataTable tbody');

        if (!thead || !tbody) { // 防御性编程，确保元素存在
            console.error("未找到表格的thead或tbody元素!");
            return;
        }

        // 渲染表头 (thead)
        thead.innerHTML = `
                <tr>
                    ${columns.map(col => `<th>${escapeHtml(col.toString())}</th>`).join('')}
                    <th>操作</th> </tr>
            `;

        // 如果没有数据，则显示提示信息
        if (currentData.length === 0) {
            tbody.innerHTML = `<tr><td colspan="${columns.length + 1}" style="text-align: center;">暂无数据</td></tr>`;
            return;
        }

        // 渲染表格数据行 (tbody)
        tbody.innerHTML = currentData.map(item => `
                <tr>
                    ${columns.map(col => `<td>${escapeHtml(item[col] !== null && item[col] !== undefined ? item[col].toString() : '')}</td>`).join('')}
                    <td>
                        <button class="button-edit" onclick="editItem('${escapeHtml(item.id.toString())}')">编辑</button>
                        <button class="button-delete" onclick="deleteItem('${escapeHtml(item.id.toString())}')">删除</button>
                    </td>
                </tr>
            `).join('');
    }

    /**
     * @brief 转义HTML特殊字符，防止XSS攻击。
     * @param {string} unsafe 可能包含不安全字符的原始字符串。
     * @returns {string} 转义后的安全字符串。
     */
    function escapeHtml(unsafe) {
        if (typeof unsafe !== 'string') { // 处理非字符串输入
            if (unsafe === null || unsafe === undefined) return ''; // null或undefined转为空字符串
            unsafe = unsafe.toString(); // 其他类型转为字符串
        }
        return unsafe
             .replace(/&/g, "&amp;")
             .replace(/</g, "&lt;")
             .replace(/>/g, "&gt;")
             .replace(/"/g, "&quot;")
             .replace(/'/g, "&#039;");
    }

    /**
     * @brief 显示用于新增配置的表单模态框。
     */
    function showAddForm() {
        isEditMode = false; // 设置为非编辑模式
        editId = null;      // 清空编辑ID
        document.getElementById('formTitle').textContent = '新增配置'; // 设置模态框标题
        document.getElementById('dataForm').reset(); // 重置表单（清空所有字段）
        generateFormFields({}); // 生成空的表单字段
        document.getElementById('formModal').classList.add('active'); // 显示模态框
        document.getElementById('overlay').classList.add('active');   // 显示遮罩层
        clearApiError(); // 打开表单时清除主界面的API错误
    }

    /**
     * @brief 显示用于编辑配置的表单模态框。
     * @param {string|number} id 要编辑的配置项的ID。
     */
    async function editItem(id) {
        isEditMode = true; // 设置为编辑模式
        editId = id;       // 存储编辑ID
        const item = currentData.find(d => d.id.toString() === id.toString()); // 从currentData中查找对应项

        if (!item) { // 如果未找到项
            displayApiError('未找到要编辑的配置项。数据可能已更新，请刷新列表。');
            return;
        }

        document.getElementById('formTitle').textContent = '编辑配置'; // 设置模态框标题
        document.getElementById('dataForm').reset();
        generateFormFields(item); // 根据查找到的项填充表单字段
        document.getElementById('formModal').classList.add('active');
        document.getElementById('overlay').classList.add('active');
        clearApiError(); // 打开表单时清除主界面的API错误
    }

    /**
     * @brief 根据提供的配置项数据动态生成表单字段。
     * @param {object} item (可选) 配置项对象，包含id, key, value。如果为空对象，则生成空表单。
     */
    function generateFormFields(item = {}) {
        const formFields = document.getElementById('formFields');
        let fieldsHtml = '';

        // Key 字段 (对于新增和编辑都是必须的)
        fieldsHtml += `<div>
                        <label for="input_key">配置键 (key)：</label>
                        <input type="text" id="input_key" name="key" value="${escapeHtml(item.key || '')}" required>
                       </div>`;
        // Value 字段 (对于新增和编辑都是必须的)
        fieldsHtml += `<div>
                        <label for="input_value">配置值 (value)：</label>
                        <input type="text" id="input_value" name="value" value="${escapeHtml(item.value || '')}" required>
                       </div>`;
        // ID 字段 (仅在编辑模式下显示且禁用)
        if (isEditMode && item.id !== undefined) { // 确保 item.id 存在
            fieldsHtml = `<div>
                            <label for="input_id">ID：</label>
                            <input type="text" id="input_id" name="id" value="${escapeHtml(item.id.toString())}" disabled>
                          </div>` + fieldsHtml; // 将ID字段放在最前面
        }
        formFields.innerHTML = fieldsHtml;
    }

    /**
     * @brief 提交表单数据 (用于新增或更新配置)。
     * 成功后会重新获取数据并关闭模态框。
     * 失败则显示错误信息。
     */
    async function submitForm() {
        clearApiError(); // 提交前清除错误
        const formData = {}; // 用于存储待提交的表单数据

        // 从表单DOM元素获取 key 和 value
        const keyInput = document.getElementById('input_key');
        const valueInput = document.getElementById('input_value');

        formData.key = keyInput ? keyInput.value.trim() : '';     // 获取并去除首尾空格
        formData.value = valueInput ? valueInput.value.trim() : ''; // 获取并去除首尾空格

        // 基本校验
        if (!formData.key) {
            alert('配置键 (key) 不能为空。'); // 使用alert提示表单内校验错误
            return;
        }
        if (!formData.value) { // 根据业务需求，value是否允许为空，此处设为必填
            alert('配置值 (value) 不能为空。');
            return;
        }

        // 根据是否为编辑模式确定API的URL和HTTP方法
        const url = isEditMode ? `/api/data/${editId}` : '/api/data';
        const method = isEditMode ? 'PUT' : 'POST';

        // 如果是编辑模式，确保 editId 有效 (虽然理论上 editItem 已保证)
        if (isEditMode && (editId === null || editId === undefined)) {
            alert('编辑错误：未指定配置项ID。');
            return;
        }

        try {
            const res = await fetch(url, {
                method,
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(formData) // 将JS对象序列化为JSON字符串
            });

            if (!res.ok) { // HTTP响应非2xx
                const errorData = await res.json().catch(() => ({ message: '操作失败，无法解析服务器响应。请检查网络或联系管理员。' }));
                throw new Error(errorData.message || `HTTP错误！状态码: ${res.status}`);
            }
            // 操作成功 (新增或更新)
            await fetchData(); // 重新获取最新数据并渲染表格
            hideForm();        // 关闭模态框
        } catch (error) {
            console.error("表单提交错误:", error);
            // 对于表单提交的错误，可以直接在模态框内或通过alert提示用户
            alert(`操作失败: ${error.message}`);
        }
    }

    /**
     * @brief 删除指定ID的配置项。
     * @param {string|number} id 要删除的配置项ID。
     */
    async function deleteItem(id) {
        clearApiError(); // 删除前清除错误
        if (confirm(`确定要删除 ID 为 ${id} 的配置项吗？此操作不可恢复。`)) { // 弹窗确认
            try {
                const res = await fetch(`/api/data/${id}`, { method: 'DELETE' }); // 调用删除API

                if (!res.ok) {
                    const errorData = await res.json().catch(() => ({ message: '删除失败，无法解析服务器响应。' }));
                    throw new Error(errorData.message || `HTTP错误！状态码: ${res.status}`);
                }
                // 删除成功
                await fetchData(); // 重新获取最新数据并渲染表格
            } catch (error) {
                console.error("删除配置项错误:", error);
                displayApiError(`删除失败: ${error.message}`); // 在主界面显示错误
            }
        }
    }

    /**
     * @brief 隐藏表单模态框和遮罩层。
     * 并重置表单内容。
     */
    function hideForm() {
        document.getElementById('formModal').classList.remove('active');
        document.getElementById('overlay').classList.remove('active');
        document.getElementById('dataForm').reset(); // 清空表单字段
    }
    // JavaScript逻辑结束
</script>
</body>
</html>
)HTML_CONTENT"; // HTML_CONTENT 是原始字符串字面量的结束分隔符
                // 设置HTTP响应头，指定内容类型为HTML，字符集为UTF-8
                res.set_content(html_content, "text/html; charset=utf-8");
            }


            /**
             * @brief 处理获取所有配置项的API请求。
             * @param req HTTP 请求对象 (当前未使用，但保留以备将来扩展，例如分页、过滤等)。
             * @param res HTTP 响应对象。
             *
             * 成功时，响应体为包含所有配置项的JSON数组。
             * 失败时，响应体为包含错误信息的JSON对象。
             */
            void getAllConfigs(const Request &req, Response &res) {
                try {
                    // 从数据库查询所有配置项，按ID升序排序
                    // db_.query 参数: 表名, 查询列(空表示所有), where条件, order by, limit, offset
                    auto result = db_.query("config", {"id", "key", "value"}, "", "id ASC", -1, -1);
                    json data = json::array(); // 初始化一个空的JSON数组

                    for (const auto &row : result) { // 遍历查询结果的每一行
                        // SQLiteDB::query 返回 vector<vector<string>>
                        // 每行 (row) 是一个 vector<string>
                        if (row.size() >= 3) { // 确保行至少包含3个字段 (id, key, value)
                            json record;
                            try {
                                // 将字符串类型的ID转换为长整型 (long long)
                                record["id"] = std::stoll(row[0]); // id 通常是第一个元素
                                record["key"] = row[1];            // key 是第二个元素
                                record["value"] = row[2];          // value 是第三个元素
                                data.push_back(record);            // 将记录添加到JSON数组
                            } catch (const std::invalid_argument& ia) {
                                std::cerr << "错误：转换ID时发生无效参数错误 (id: '" << row[0] << "'): " << ia.what() << std::endl;
                            } catch (const std::out_of_range& oor) {
                                std::cerr << "错误：转换ID时发生越界错误 (id: '" << row[0] << "'): " << oor.what() << std::endl;
                            }
                        } else {
                            std::cerr << "警告：从数据库读取的行数据列数不足 (预期至少3列，实际" << row.size() << "列)。" << std::endl;
                        }
                    }
                    // 设置响应内容为JSON字符串，并指定字符集
                    res.set_content(data.dump(4), "application/json; charset=utf-8"); // dump(4) 用于格式化JSON输出（带缩进）
                } catch (const std::exception &e) { // 捕获所有SQLiteDB可能抛出的std::exception (包括std::runtime_error)
                    std::cerr << "错误 (getAllConfigs): " << e.what() << std::endl;
                    res.status = 500; // 内部服务器错误
                    json error_res = {{"message", "获取配置列表失败: " + std::string(e.what())}};
                    res.set_content(error_res.dump(), "application/json; charset=utf-8");
                }
            }

            /**
             * @brief 处理新增配置项的API请求。
             * @param req HTTP 请求对象，请求体应为包含 "key" 和 "value" 的JSON对象。
             * @param res HTTP 响应对象。
             *
             * 成功时，返回所有配置项的最新列表。
             * 失败时 (如JSON格式错误、缺少字段、数据库错误)，返回相应的错误信息。
             */
            void addConfig(const Request &req, Response &res) {
                try {
                    auto jsonData = json::parse(req.body); // 解析请求体中的JSON数据
                    std::string key = jsonData.at("key").get<std::string>();
                    std::string value = jsonData.at("value").get<std::string>();

                    if (key.empty()) {
                        res.status = 400; // 错误请求
                        json error_res = {{"message", "配置键 (key) 不能为空"}};
                        res.set_content(error_res.dump(), "application/json; charset=utf-8");
                        return;
                    }
                    if (value.empty()) {
                        res.status = 400;
                        json error_res = {{"message", "配置值 (value) 不能为空"}};
                        res.set_content(error_res.dump(), "application/json; charset=utf-8");
                        return;
                    }

                    // 向数据库插入新记录。SQLiteDB::insert 接受 vector<vector<string>>
                    // {{key, value}} 会隐式转换为 vector<vector<string>>{ vector<string>{key, value} }
                    db_.insert("config", {"key", "value"}, {{key, value}});
                    returnAllConfigs(res); // 返回更新后的所有配置项列表
                }
                catch (const json::parse_error &e) { // JSON解析失败
                    res.status = 400;
                    json error_res = {{"message", "无效的JSON格式: " + std::string(e.what())}};
                    res.set_content(error_res.dump(), "application/json; charset=utf-8");
                }
                catch (const json::type_error &e) { // JSON字段缺失或类型不匹配 (例如jsonData.at("key")失败)
                    res.status = 400;
                    json error_res = {{"message", "请求的JSON中缺少 'key' 或 'value' 字段，或字段类型不正确: " + std::string(e.what())}};
                    res.set_content(error_res.dump(), "application/json; charset=utf-8");
                }
                    // 捕获SQLiteDB可能抛出的std::runtime_error (如UNIQUE约束) 和 std::invalid_argument (虽然此处调用不太可能触发)
                catch (const std::runtime_error &e) {
                    std::string errMsg = e.what();
                    // SQLiteDB::executeSQL 在 UNIQUE constraint failed 时会抛出包含 "UNIQUE constraint failed" 的 std::runtime_error
                    if (errMsg.find("UNIQUE constraint failed") != std::string::npos || errMsg.find("UNIQUE constraint") != std::string::npos) {
                        res.status = 409; // 冲突 (Conflict)
                        std::string conflictingKey = "未知";
                        try { conflictingKey = json::parse(req.body).value("key", "未知"); } catch(...) {}
                        json error_res = {{"message", "配置键 '" + conflictingKey + "' 已存在，无法重复添加。" }};
                        res.set_content(error_res.dump(), "application/json; charset=utf-8");
                    } else {
                        res.status = 500; // 其他数据库错误视为内部服务器错误
                        json error_res = {{"message", "添加配置时数据库操作失败: " + errMsg}};
                        res.set_content(error_res.dump(), "application/json; charset=utf-8");
                    }
                }
                catch (const std::invalid_argument &e) { // 由SQLiteDB::insert参数校验抛出 (理论上此处不会)
                    res.status = 400;
                    json error_res = {{"message", "提交的数据格式不正确 (列/值不匹配): " + std::string(e.what())}};
                    res.set_content(error_res.dump(), "application/json; charset=utf-8");
                }
                catch (const std::exception &e) { // 捕获其他所有标准异常
                    std::cerr << "错误 (addConfig): " << e.what() << std::endl;
                    res.status = 500;
                    json error_res = {{"message", "添加配置失败: " + std::string(e.what())}};
                    res.set_content(error_res.dump(), "application/json; charset=utf-8");
                }
            }

            /**
             * @brief 处理更新指定ID配置项的API请求。
             * @param req HTTP 请求对象。URL路径参数 ":id" 指定要更新的配置项ID。
             * 请求体应为包含 "key" 和 "value" 的JSON对象。
             * @param res HTTP 响应对象。
             *
             * 成功时，返回所有配置项的最新列表。
             * 失败时 (如ID无效、JSON格式错误、记录不存在、数据库错误)，返回相应的错误信息。
             */
            void updateConfig(const Request &req, Response &res) {
                long long id_val = 0; // 用于存储从路径参数转换得到的ID
                try {
                    if (!req.has_param("id")) {
                        res.status = 400; // 错误请求
                        json error_res = {{"message", "URL路径中缺少ID参数。"}};
                        res.set_content(error_res.dump(), "application/json; charset=utf-8");
                        return;
                    }
                    id_val = std::stoll(req.path_params.at("id"));

                    auto jsonData = json::parse(req.body);
                    std::string key = jsonData.at("key").get<std::string>();
                    std::string value = jsonData.at("value").get<std::string>();

                    if (key.empty()) {
                        res.status = 400;
                        json error_res = {{"message", "配置键 (key) 不能为空"}};
                        res.set_content(error_res.dump(), "application/json; charset=utf-8");
                        return;
                    }
                    if (value.empty()) {
                        res.status = 400;
                        json error_res = {{"message", "配置值 (value) 不能为空"}};
                        res.set_content(error_res.dump(), "application/json; charset=utf-8");
                        return;
                    }

                    auto existing = db_.query("config", {"id"}, "id = " + std::to_string(id_val), "", 1, 0);
                    if (existing.empty()) {
                        res.status = 404; // 未找到 (Not Found)
                        json error_res = {{"message", "未找到ID为 " + std::to_string(id_val) + " 的配置项，无法更新。"}};
                        res.set_content(error_res.dump(), "application/json; charset=utf-8");
                        return;
                    }

                    // 更新数据库中的记录。SQLiteDB::update 接受 vector<string> 作为值
                    db_.update("config", {"key", "value"}, {key, value}, "id = " + std::to_string(id_val));
                    returnAllConfigs(res);
                }
                catch (const std::invalid_argument& e) { // stoll 转换ID时的无效参数错误 或 SQLiteDB::update的参数错误
                    res.status = 400;
                    std::string error_what = e.what();
                    if (error_what.find("stoll") != std::string::npos || error_what.find("ID") != std::string::npos) { // 假设stoll的异常信息包含"stoll"
                        json error_res = {{"message", "提供的ID参数不是一个有效的数字。" }};
                        res.set_content(error_res.dump(), "application/json; charset=utf-8");
                    } else { // SQLiteDB::update 抛出的 invalid_argument
                        json error_res = {{"message", "更新数据格式不正确 (列/值不匹配): " + error_what}};
                        res.set_content(error_res.dump(), "application/json; charset=utf-8");
                    }
                } catch (const std::out_of_range& oor) { // stoll 转换ID时的越界错误 或 req.path_params.at("id") / jsonData.at() 失败
                    res.status = 400;
                    std::string error_what = oor.what();
                    if (error_what.find("stoll") != std::string::npos || error_what.find("ID") != std::string::npos) {
                        json error_res = {{"message", "提供的ID值过大或过小。" }};
                        res.set_content(error_res.dump(), "application/json; charset=utf-8");
                    } else if (error_what.find("key") != std::string::npos || error_what.find("value") != std::string::npos) { // json.at() 失败
                        json error_res = {{"message", "请求的JSON中缺少必须的字段 (如key或value)。"}};
                        res.set_content(error_res.dump(), "application/json; charset=utf-8");
                    } else { // 理论上是 req.path_params.at("id") 失败，但已用 has_param 检查
                        json error_res = {{"message", "获取路径或JSON参数时发生越界错误。"}};
                        res.set_content(error_res.dump(), "application/json; charset=utf-8");
                    }
                }
                catch (const json::parse_error &e) {
                    res.status = 400;
                    json error_res = {{"message", "无效的JSON格式: " + std::string(e.what())}};
                    res.set_content(error_res.dump(), "application/json; charset=utf-8");
                }
                catch (const json::type_error &e) { // json.get<std::string>() 类型转换失败
                    res.status = 400;
                    json error_res = {{"message", "请求的JSON中字段类型不正确: " + std::string(e.what())}};
                    res.set_content(error_res.dump(), "application/json; charset=utf-8");
                }
                catch (const std::runtime_error &e) { // SQLite的UNIQUE约束等数据库错误
                    std::string errMsg = e.what();
                    if (errMsg.find("UNIQUE constraint failed") != std::string::npos || errMsg.find("UNIQUE constraint") != std::string::npos) {
                        res.status = 409; // 冲突
                        std::string conflictingKey = "未知";
                        try { conflictingKey = json::parse(req.body).value("key", "未知"); } catch(...) {}
                        json error_res = {{"message", "配置键 '" + conflictingKey + "' 已被其他配置项使用。" }};
                        res.set_content(error_res.dump(), "application/json; charset=utf-8");
                    } else {
                        res.status = 500;
                        json error_res = {{"message", "更新配置时数据库操作失败: " + errMsg}};
                        res.set_content(error_res.dump(), "application/json; charset=utf-8");
                    }
                }
                catch (const std::exception &e) {
                    std::cerr << "错误 (updateConfig, id: " << id_val << "): " << e.what() << std::endl;
                    res.status = 500;
                    json error_res = {{"message", "更新配置失败: " + std::string(e.what())}};
                    res.set_content(error_res.dump(), "application/json; charset=utf-8");
                }
            }

            /**
             * @brief 处理删除指定ID配置项的API请求。
             * @param req HTTP 请求对象。URL路径参数 ":id" 指定要删除的配置项ID。
             * @param res HTTP 响应对象。
             *
             * 成功时，返回所有配置项的最新列表。
             * 失败时 (如ID无效、记录不存在、数据库错误)，返回相应的错误信息。
             */
            void deleteConfig(const Request &req, Response &res) {
                long long id_val = 0;
                try {
                    if (!req.has_param("id")) {
                        res.status = 400;
                        json error_res = {{"message", "URL路径中缺少ID参数。"}};
                        res.set_content(error_res.dump(), "application/json; charset=utf-8");
                        return;
                    }
                    id_val = std::stoll(req.path_params.at("id"));

                    auto existing = db_.query("config", {"id"}, "id = " + std::to_string(id_val), "", 1, 0);
                    if (existing.empty()) {
                        res.status = 404; // 未找到
                        json error_res = {{"message", "未找到ID为 " + std::to_string(id_val) + " 的配置项，无法删除。"}};
                        res.set_content(error_res.dump(), "application/json; charset=utf-8");
                        return;
                    }

                    db_.remove("config", "id = " + std::to_string(id_val));
                    returnAllConfigs(res);
                }
                catch (const std::invalid_argument& ia_id) { // stoll
                    res.status = 400;
                    json error_res = {{"message", "提供的ID参数不是一个有效的数字。" }};
                    res.set_content(error_res.dump(), "application/json; charset=utf-8");
                } catch (const std::out_of_range& oor_id) { // stoll or req.path_params.at("id")
                    res.status = 400;
                    std::string error_what = oor_id.what();
                    if (error_what.find("stoll") != std::string::npos || error_what.find("ID") != std::string::npos) {
                        json error_res = {{"message", "提供的ID值过大或过小。" }};
                        res.set_content(error_res.dump(), "application/json; charset=utf-8");
                    } else {
                        json error_res = {{"message", "获取ID路径参数时发生错误。"}};
                        res.set_content(error_res.dump(), "application/json; charset=utf-8");
                    }
                }
                catch (const std::exception &e) { // 捕获SQLiteDB可能抛出的std::runtime_error等
                    std::cerr << "错误 (deleteConfig, id: " << id_val << "): " << e.what() << std::endl;
                    res.status = 500;
                    json error_res = {{"message", "删除配置失败: " + std::string(e.what())}};
                    res.set_content(error_res.dump(), "application/json; charset=utf-8");
                }
            }

            /**
             * @brief 内部辅助方法：获取并返回所有配置项的JSON列表。
             *
             * 用于在新增、更新、删除操作成功后，统一返回最新的数据给客户端。
             * @param res HTTP 响应对象。
             */
            void returnAllConfigs(Response &res) {
                try {
                    auto result = db_.query("config", {"id", "key", "value"}, "", "id ASC", -1, -1);
                    json data = json::array();
                    for (const auto &row: result) {
                        if (row.size() >= 3) {
                            json record;
                            try {
                                record["id"] = std::stoll(row[0]);
                                record["key"] = row[1];
                                record["value"] = row[2];
                                data.push_back(record);
                            } catch (const std::exception& e_conv) {
                                std::cerr << "错误 (returnAllConfigs - 转换行数据时, id: " << row[0] << "): " << e_conv.what() << std::endl;
                            }
                        }
                    }
                    res.set_content(data.dump(4), "application/json; charset=utf-8");
                } catch (const std::exception &e) { // 捕获SQLiteDB::query可能抛出的异常
                    std::cerr << "错误 (returnAllConfigs - 获取数据时): " << e.what() << std::endl;
                    res.status = 500;
                    json error_res = {{"message", "获取最新配置列表时发生内部错误: " + std::string(e.what())}};
                    res.set_content(error_res.dump(), "application/json; charset=utf-8");
                }
            }

        private:
            Server svr_;                            ///< httplib 的 HTTP 服务器实例。
            LSX_LIB::SQL::SQLiteDB db_;             ///< SQLite 数据库操作实例
            int port_;                              ///< 服务器监听的端口号。
            std::string baseDir_;                   ///< 静态文件服务的基础目录。
        }; // class ConfigServer 结束
    } // namespace Config 结束
} // namespace LSX_LIB 结束

#endif // CONFIG_SERVER_H