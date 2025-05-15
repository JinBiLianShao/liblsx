/**
 * @file ConfigServer.h
 * @brief HTTP 服务器封装类，提供配置项的 CRUD 操作接口
 * @author 连思鑫
 */

#include <iostream>
#include <string>
#include <httplib.h>
#include <nlohmann/json.hpp>
#include "SQLiteDB.h"

using namespace httplib;
using json = nlohmann::json;

/**
 * @class ConfigServer
 * @brief 基于 HTTP 的配置项管理服务器
 *
 * 这个类封装了一个简单的 HTTP 服务器，提供对 SQLite 数据库中配置项的
 * CRUD (创建、读取、更新、删除) 操作接口。服务器同时提供静态文件服务能力。
 *
 * @example 基本使用示例
 * @code
 * int main() {
 *     // 创建服务器实例，指定数据库文件、静态文件目录和端口
 *     ConfigServer server("data.db", ".", 3000);
 *
 *     // 初始化数据库和路由
 *     server.initialize();
 *
 *     // 启动服务器
 *     server.run();
 *
 *     return 0;
 * }
 * @endcode
 *
 * @example 自定义路由处理示例
 * @code
 * int main() {
 *     ConfigServer server("data.db");
 *
 *     // 初始化后添加自定义路由
 *     server.initialize();
 *     server.addCustomRoute("GET", "/custom", [](const Request& req, Response& res) {
 *         res.set_content("Custom Response", "text/plain");
 *     });
 *
 *     server.run();
 *     return 0;
 * }
 * @endcode
 */
/**
 * @brief LSX 库的根命名空间。
 */
namespace LSX_LIB::Config{
    class ConfigServer {
    public:
        /**
         * @brief 构造函数
         * @param dbPath SQLite 数据库文件路径
         * @param baseDir 静态文件服务的基础目录（默认为当前目录）
         * @param port 服务器监听端口（默认为3000）
         */
        ConfigServer(const std::string &dbPath, const std::string &baseDir = ".", int port = 3000)
                : db_(dbPath), port_(port), baseDir_(baseDir) {
        }

        /**
         * @brief 初始化服务器
         *
         * 执行以下初始化操作：
         * 1. 初始化数据库表结构
         * 2. 设置默认路由
         * 3. 配置静态文件服务
         */
        void initialize() {
            initializeDatabase();
            setupRoutes();
            serveStaticFiles();
        }

        /**
         * @brief 启动服务器
         *
         * 启动 HTTP 服务器并开始监听指定端口
         */
        void run() {
            std::cout << "Server running on http://localhost:" << port_ << std::endl;
            svr_.listen("0.0.0.0", port_);
        }

        /**
         * @brief 添加自定义路由
         * @param method HTTP 方法（"GET", "POST", "PUT", "DELETE"等）
         * @param pattern 路由模式（支持参数，如"/path/:id"）
         * @param handler 请求处理函数
         *
         * @example 添加自定义路由
         * @code
         * server.addCustomRoute("GET", "/status", [](const auto& req, auto& res) {
         *     res.set_content("Server is running", "text/plain");
         * });
         * @endcode
         */
        void addCustomRoute(const std::string &method, const std::string &pattern,
                            std::function<void(const Request &, Response &)> handler) {
            if (method == "GET") {
                svr_.Get(pattern, handler);
            } else if (method == "POST") {
                svr_.Post(pattern, handler);
            } else if (method == "PUT") {
                svr_.Put(pattern, handler);
            } else if (method == "DELETE") {
                svr_.Delete(pattern, handler);
            } else {
                throw std::runtime_error("Unsupported HTTP method: " + method);
            }
        }

    private:
        /// @brief 初始化数据库表结构
        void initializeDatabase() {
            if (!db_.tableExists("config")) {
                std::cout << "Creating table 'config'..." << std::endl;
                db_.createTable("config", {
                        "id INTEGER PRIMARY KEY AUTOINCREMENT",
                        "key TEXT NOT NULL",
                        "value TEXT NOT NULL"
                });
            } else {
                std::cout << "Table 'config' already exists." << std::endl;
            }
        }

        /// @brief 设置默认路由
        void setupRoutes() {
            // 首页路由
            svr_.Get("/", [this](const Request &req, Response &res) {
                serveIndexPage(res);
            });

            // 配置项API路由
            svr_.Get("/api/data", [this](const Request &req, Response &res) {
                getAllConfigs(res);
            });

            svr_.Post("/api/data", [this](const Request &req, Response &res) {
                addConfig(req, res);
            });

            svr_.Put("/api/data/:id", [this](const Request &req, Response &res) {
                updateConfig(req, res);
            });

            svr_.Delete("/api/data/:id", [this](const Request &req, Response &res) {
                deleteConfig(req, res);
            });
        }

        /// @brief 配置静态文件服务
        void serveStaticFiles() {
            svr_.set_base_dir(baseDir_.c_str());
        }

        /**
         * @brief 提供首页HTML文件
         * @param res HTTP响应对象
         */
        void serveIndexPage(Response &res) {
            std::ifstream file("./index.html");
            if (file.is_open()) {
                std::stringstream buffer;
                buffer << file.rdbuf();
                res.set_content(buffer.str(), "text/html");
            } else {
                res.status = 404;
                res.set_content("Not found", "text/plain");
            }
        }

        /**
         * @brief 获取所有配置项
         * @param res HTTP响应对象
         *
         * 响应格式：
         * @code
         * [
         *     {"id": 1, "key": "name", "value": "John"},
         *     {"id": 2, "key": "age", "value": "30"}
         * ]
         * @endcode
         */
        void getAllConfigs(Response &res) {
            auto result = db_.query("config", {}, "", "", -1, -1);
            json data = json::array();
            for (const auto &row: result) {
                json record = {
                        {"id",    row[0]},
                        {"key",   row[1]},
                        {"value", row[2]}
                };
                data.push_back(record);
            }
            res.set_content(data.dump(), "application/json");
        }

        /**
         * @brief 添加新配置项
         * @param req HTTP请求对象
         * @param res HTTP响应对象
         *
         * 请求体格式：
         * @code
         * {"key": "new_key", "value": "new_value"}
         * @endcode
         */
        void addConfig(const Request &req, Response &res) {
            try {
                auto jsonData = json::parse(req.body);
                std::string key = jsonData["key"];
                std::string value = jsonData["value"];

                db_.insert("config", {"key", "value"}, {{key, value}});
                returnAllConfigs(res);
            }
            catch (const std::exception &e) {
                res.status = 400;
                res.set_content("Invalid JSON", "text/plain");
            }
        }

        /**
         * @brief 更新配置项
         * @param req HTTP请求对象
         * @param res HTTP响应对象
         *
         * URL参数: :id - 要更新的配置项ID
         * 请求体格式：
         * @code
         * {"key": "updated_key", "value": "updated_value"}
         * @endcode
         */
        void updateConfig(const Request &req, Response &res) {
            try {
                int id = std::stoi(req.path_params.at("id"));
                auto jsonData = json::parse(req.body);
                std::string key = jsonData["key"];
                std::string value = jsonData["value"];

                db_.update("config", {"key", "value"}, {key, value}, "id = " + std::to_string(id));
                returnAllConfigs(res);
            }
            catch (const std::exception &e) {
                res.status = 400;
                res.set_content("Invalid JSON", "text/plain");
            }
        }

        /**
         * @brief 删除配置项
         * @param req HTTP请求对象
         * @param res HTTP响应对象
         *
         * URL参数: :id - 要删除的配置项ID
         */
        void deleteConfig(const Request &req, Response &res) {
            try {
                int id = std::stoi(req.path_params.at("id"));
                db_.remove("config", "id = " + std::to_string(id));
                returnAllConfigs(res);
            }
            catch (const std::exception &e) {
                res.status = 400;
                res.set_content("Invalid ID", "text/plain");
            }
        }

        /// @brief 返回所有配置项（内部辅助方法）
        void returnAllConfigs(Response &res) {
            auto result = db_.query("config", {}, "", "", -1, -1);
            json data = json::array();
            for (const auto &row: result) {
                json record = {
                        {"id",    row[0]},
                        {"key",   row[1]},
                        {"value", row[2]}
                };
                data.push_back(record);
            }
            res.set_content(data.dump(), "application/json");
        }

    private:
        Server svr_; ///< HTTP 服务器实例
        LSX_LIB::SQLiteDB db_; ///< SQLite 数据库操作实例
        int port_; ///< 服务器监听端口
        std::string baseDir_; ///< 静态文件服务基础目录
    };
}