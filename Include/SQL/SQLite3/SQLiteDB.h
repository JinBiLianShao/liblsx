/**
 * @file SQLiteDB.h
 * @brief 线程安全的SQLite数据库操作类，封装了SQLite的常用功能。
 * @author 连思鑫
 * @date 2025-1-22 (优化: 2025-06-09, 修复事务嵌套: 2025-07-04)
 *
 * @details
 * 该类封装了SQLite数据库的常用操作，包括连接/关闭数据库、创建表、删除表、清空表、
 * 插入数据、删除数据、更新数据、查询数据、检查表是否存在以及获取列名。
 *
 * 通过在所有公共数据库操作方法中加入 `std::recursive_mutex`，此类实现了线程安全，
 * 允许单个SQLiteDB实例在多线程环境下被安全地共享和使用。
 *
 * 同时，该类通过RAII机制管理事务，并通过预编译语句（Prepared Statements）来防止SQL注入，
 * 提高了代码的健壮性和安全性。
 *
 * @example
 * 以下是一个详细的使用示例，展示了SQLiteDB类的主要功能和推荐用法：
 * @code
 * #include <iostream>
 * #include <vector>
 * #include <string>
 * #include <stdexcept>
 * #include <thread> // 用于多线程示例
 * #include "SQLiteDB.h" // 确保包含您的SQLiteDB头文件
 *
 * // 在多线程环境中共享同一个数据库连接对象
 * void thread_task(LSX_LIB::SQLiteDB& db, int thread_id) {
 * try {
 * std::string name = "User_T" + std::to_string(thread_id);
 * std::string age = std::to_string(20 + thread_id);
 * db.insert("users", {"name", "age"}, {{name, age}});
 * std::cout << "线程 " << thread_id << " 插入数据成功。" << std::endl;
 * } catch (const std::exception& e) {
 * std::cerr << "线程 " << thread_id << " 操作失败: " << e.what() << std::endl;
 * }
 * }
 *
 * int main() {
 * try {
 * // 1. 数据库连接与初始化
 * // 尝试打开或创建一个名为 "example.db" 的SQLite数据库文件。
 * LSX_LIB::SQLiteDB db("example.db");
 *
 * // 2. 表存在性检查与创建
 * if (!db.tableExists("users")) {
 * std::cout << "创建表 'users'..." << std::endl;
 * db.createTable("users", {"id INTEGER PRIMARY KEY AUTOINCREMENT", "name TEXT NOT NULL", "age INTEGER"});
 * } else {
 * std::cout << "表 'users' 已存在。" << std::endl;
 * db.clearTable("users"); // 清空表以便演示
 * }
 *
 * // 3. 插入数据（支持批量插入）
 * std::cout << "插入数据..." << std::endl;
 * db.insert("users", {"name", "age"}, {{"Alice", "30"}, {"Bob", "25"}, {"Charlie", "22"}});
 *
 * // 4. 查询数据
 * std::cout << "查询年龄大于25的用户，并按年龄降序排列:" << std::endl;
 * auto result = db.query("users", {"name", "age"}, "age > 25", "age DESC");
 * for (const auto& row : result) {
 * std::cout << row[0] << " " << row[1] << std::endl;
 * }
 *
 * // 5. 更新数据
 * std::cout << "更新 Alice 的年龄为 31..." << std::endl;
 * db.update("users", {"age"}, {"31"}, "name = 'Alice'");
 *
 * // 6. 获取列名
 * std::cout << "表 'users' 的列名:" << std::endl;
 * std::vector<std::string> columnNames = db.getColumnNames("users");
 * for (const auto& colName : columnNames) {
 * std::cout << colName << " ";
 * }
 * std::cout << std::endl << std::endl;
 *
 * // 7. 使用事务批量操作
 * std::cout << "使用事务插入更多数据..." << std::endl;
 * {
 * LSX_LIB::SQLiteDB::Transaction transaction(db); // 开启事务
 * db.insert("users", {"name", "age"}, {{"David", "28"}, {"Eve", "24"}});
 * transaction.commit(); // 显式提交事务
 * } // transaction 对象在此处析构，如果未调用commit()将自动回滚
 *
 * // 8. 多线程安全测试
 * std::cout << "启动多线程测试..." << std::endl;
 * std::vector<std::thread> threads;
 * for (int i = 0; i < 5; ++i) {
 * threads.emplace_back(thread_task, std::ref(db), i);
 * }
 * for (auto& t : threads) {
 * if (t.joinable()) {
 * t.join();
 * }
 * }
 * std::cout << "多线程测试完成。" << std::endl << std::endl;
 *
 * // 9. 查询所有数据以验证变更
 * std::cout << "查询 'users' 表中的所有数据:" << std::endl;
 * result = db.query("users", {}, "", "id ASC");
 * for (const auto& row : result) {
 * for (const auto& col : row) {
 * std::cout << col << "\t";
 * }
 * std::cout << std::endl;
 * }
 *
 * } catch (const std::exception& e) {
 * std::cerr << "发生错误: " << e.what() << std::endl;
 * return 1;
 * }
 *
 * std::cout << "\n数据库操作成功完成。" << std::endl;
 * return 0;
 * }
 * @endcode
 */
#ifndef LSX_LIB_SQLITEDB_H
#define LSX_LIB_SQLITEDB_H

#include <sqlite3.h> // SQLite C API
#include <iostream>  // 用于控制台输出
#include <string>    // 用于字符串操作
#include <vector>    // 用于动态数组
#include <stdexcept> // 用于标准异常
#include <mutex>     // 用于线程互斥锁
#include <memory>    // 尽管此处未使用unique_ptr/shared_ptr管理db_，但保留以备未来扩展

// 添加命名空间，防止类名冲突，提高代码组织性
namespace LSX_LIB
{
    /**
     * @class SQLiteDB
     * @brief 线程安全的SQLite数据库操作类，封装了常用的数据库功能。
     *
     * @details
     * 该类封装了SQLite数据库的常用操作，包括创建表、删除表、清空表、
     * 插入数据、删除数据、更新数据、查询数据、检查表是否存在以及获取列名。
     *
     * 通过递归互斥锁（`std::recursive_mutex`）和 SQLite 的 `SQLITE_OPEN_FULLMUTEX` 模式，
     * 确保所有数据库操作的线程安全。
     * 支持RAII风格的事务管理，确保数据操作的原子性。
     * 所有涉及预编译语句的操作都采用 RAII 方式管理 `sqlite3_stmt` 句柄，避免资源泄露。
     */
    class SQLiteDB {
    public:
        /**
         * @brief 构造函数，打开或创建数据库文件，并初始化数据库连接。
         *
         * @param dbName 数据库文件名（例如 "my_database.db"）。
         * @throws std::runtime_error 如果无法打开或创建数据库。
         */
        explicit SQLiteDB(const std::string& dbName) : dbName_(dbName), db_(nullptr) {
            // 在构造函数开始时加锁，虽然在对象首次构造时多线程访问的风险较低，
            // 但提供了一致的互斥保护，特别是当构造过程中可能调用其他私有方法时。
            std::lock_guard<std::recursive_mutex> lock(dbMutex_);

            // 以读写、创建模式打开数据库，并启用 SQLITE_OPEN_FULLMUTEX。
            // SQLITE_OPEN_FULLMUTEX 标志指示 SQLite 库在内部启用最强的线程安全模式，
            // 使得单个数据库连接可以在多个线程之间安全共享，配合外部的 std::recursive_mutex
            // 提供双重保障，确保应用层和库层面的线程安全。
            int rc = sqlite3_open_v2(dbName.c_str(), &db_,
                                      SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, nullptr);
            if (rc != SQLITE_OK) {
                // 如果打开失败，获取错误消息，并确保关闭可能的半开连接。
                std::string errMsg = "无法打开数据库: " + std::string(sqlite3_errmsg(db_));
                if (db_) sqlite3_close(db_); // 确保在抛出异常前关闭句柄
                db_ = nullptr; // 将句柄置空
                throw std::runtime_error(errMsg);
            }

            // 数据库成功打开后，立即启用外键约束。
            // 外键约束在 SQLite 中默认是关闭的，需要显式开启以维护数据引用完整性。
            // executeSQL 内部会再次加锁，但由于 dbMutex_ 是递归的，这不会造成死锁。
            executeSQL("PRAGMA foreign_keys = ON;");
            std::cout << "数据库打开成功: " << dbName_ << std::endl;
        }

        /**
         * @brief 析构函数，负责安全地关闭数据库连接。
         *
         * @details
         * 在关闭数据库前，会迭代并终结所有尚未释放的预编译语句 (sqlite3_stmt)。
         * 这对于防止资源泄露和确保程序正确退出至关重要。
         */
        ~SQLiteDB() {
            // 在析构函数中加锁，确保在关闭数据库时没有其他线程正在使用它。
            std::lock_guard<std::recursive_mutex> lock(dbMutex_);
            if (db_) {
                // 迭代并终结所有尚未释放的预编译语句。
                // 即使某些操作因异常中止，或编码中忘记 finalize，此循环也能清理它们。
                sqlite3_stmt* stmt;
                while ((stmt = sqlite3_next_stmt(db_, nullptr)) != nullptr) {
                    sqlite3_finalize(stmt);
                }
                sqlite3_close(db_); // 关闭数据库连接
                db_ = nullptr; // 将句柄置空，防止悬空指针
                std::cout << "数据库连接已关闭。" << std::endl;
            }
        }

        // 禁用拷贝构造函数和拷贝赋值操作符。
        // SQLiteDB 实例管理着一个唯一的数据库连接句柄，不应被复制。
        // 复制可能导致多个对象管理同一个句柄，从而引发双重关闭等资源管理问题。
        SQLiteDB(const SQLiteDB&) = delete;
        SQLiteDB& operator=(const SQLiteDB&) = delete;

        /**
         * @brief 检查指定名称的表是否存在于数据库中。
         *
         * @param tableName 要检查的表名。
         * @return bool 如果表存在返回 true，否则返回 false。
         * @throws std::runtime_error 如果数据库连接无效或查询失败。
         */
        bool tableExists(const std::string& tableName) {
            std::lock_guard<std::recursive_mutex> lock(dbMutex_); // 线程安全锁
            // 防御性检查：确保数据库连接在操作前是开放和有效的。
            if (!db_) {
                throw std::runtime_error("Database connection is closed.");
            }

            // 查询 sqlite_master 表以检查表是否存在。使用 ? 占位符进行参数绑定，防止 SQL 注入。
            std::string sql = "SELECT name FROM sqlite_master WHERE type='table' AND name=?;";
            sqlite3_stmt* stmt = nullptr; // 初始化为 nullptr，以防 prepare 失败

            try {
                // 预编译 SQL 语句
                if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
                    throw std::runtime_error("预编译 'tableExists' 语句失败: " +
                                             std::string(sqlite3_errmsg(db_)));
                }

                // 绑定表名参数。SQLITE_TRANSIENT 告诉 SQLite 在内部复制字符串，确保字符串生命周期。
                sqlite3_bind_text(stmt, 1, tableName.c_str(), -1, SQLITE_TRANSIENT);

                // 执行语句。如果返回 SQLITE_ROW，表示找到了匹配的行，即表存在。
                bool exists = (sqlite3_step(stmt) == SQLITE_ROW);

                sqlite3_finalize(stmt); // 释放预编译语句句柄
                return exists;
            } catch (...) {
                // 捕获所有异常，确保在异常发生时也能正确释放语句句柄。
                if (stmt) sqlite3_finalize(stmt);
                throw; // 重新抛出原始异常
            }
        }

        /**
         * @brief 创建一个新表。如果同名表已存在，则不会重复创建（使用 IF NOT EXISTS）。
         *
         * @param tableName 要创建的表名。
         * @param columns 一个字符串向量，每个字符串代表一列的定义（例如："id INTEGER PRIMARY KEY AUTOINCREMENT"）。
         * @throws std::runtime_error 如果数据库连接无效或创建表失败。
         */
        void createTable(const std::string& tableName, const std::vector<std::string>& columns) {
            std::lock_guard<std::recursive_mutex> lock(dbMutex_); // 线程安全锁
            if (!db_) {
                throw std::runtime_error("Database connection is closed.");
            }

            // 构建 CREATE TABLE IF NOT EXISTS 语句，使用 escapeIdentifier 转义表名。
            std::string sql = "CREATE TABLE IF NOT EXISTS " + escapeIdentifier(tableName) + " (";
            for (size_t i = 0; i < columns.size(); ++i) {
                sql += columns[i]; // 列定义通常由代码提供，假设是安全的。
                if (i < columns.size() - 1) {
                    sql += ", ";
                }
            }
            sql += ");";

            executeSQL(sql); // 执行 SQL 语句
        }

        /**
         * @brief 删除指定名称的表。如果表不存在，此操作不会报错（使用 IF EXISTS）。
         *
         * @param tableName 要删除的表名。
         * @throws std::runtime_error 如果数据库连接无效或删除表失败。
         */
        void dropTable(const std::string& tableName) {
            std::lock_guard<std::recursive_mutex> lock(dbMutex_); // 线程安全锁
            if (!db_) {
                throw std::runtime_error("Database connection is closed.");
            }
            // 构建 DROP TABLE IF EXISTS 语句，使用 escapeIdentifier 转义表名。
            std::string sql = "DROP TABLE IF EXISTS " + escapeIdentifier(tableName) + ";";
            executeSQL(sql); // 执行 SQL 语句
        }

        /**
         * @brief 清空指定表中的所有数据行，并重置其自增序列（如果存在）。
         *
         * @param tableName 要清空的表名。
         * @throws std::runtime_error 如果数据库连接无效或清空操作失败。
         */
        void clearTable(const std::string& tableName) {
            std::lock_guard<std::recursive_mutex> lock(dbMutex_); // 线程安全锁
            if (!db_) {
                throw std::runtime_error("Database connection is closed.");
            }
            // 使用 DELETE FROM 清空表数据。
            std::string sql = "DELETE FROM " + escapeIdentifier(tableName) + ";";
            executeSQL(sql);

            // 同时重置该表的自增序列。这对于确保新的自增 ID 从 1 开始非常重要。
            sql = "DELETE FROM sqlite_sequence WHERE name = '" + escapeString(tableName) + "';";
            executeSQL(sql);
        }

        /**
         * @brief 获取指定表的列名。
         *
         * @param tableName 要获取列名的表名。
         * @return std::vector<std::string> 包含列名的向量。
         * @throws std::runtime_error 如果数据库连接无效或无法获取列信息。
         */
        std::vector<std::string> getColumnNames(const std::string& tableName) {
            std::lock_guard<std::recursive_mutex> lock(dbMutex_); // 线程安全锁
            if (!db_) {
                throw std::runtime_error("Database connection is closed.");
            }
            std::vector<std::string> columnNames;
            // PRAGMA table_info(table_name) 返回表的结构信息，其中 'name' 列（索引为1）是列名。
            std::string sql = "PRAGMA table_info(" + escapeIdentifier(tableName) + ");";

            auto result = executeQuery(sql); // 执行查询
            for(const auto& row : result) {
                // PRAGMA table_info 的结果集中，列名位于第二列（索引为 1）
                if (row.size() > 1) {
                    columnNames.push_back(row[1]);
                }
            }
            return columnNames;
        }

        /**
         * @brief 插入数据（支持批量插入）。使用预编译语句以提高性能和安全性。
         * @note 此方法不再内部管理事务，而是依赖于外部的 `SQLiteDB::Transaction` 对象进行事务控制。
         * 这意味着如果您要进行批量插入并确保原子性，请在外部将其封装在 `SQLiteDB::Transaction` 块中。
         *
         * @param tableName 表名。
         * @param columns 要插入的列名列表。
         * @param values 要插入的值，每个内层 vector 代表一行数据。
         * @throws std::invalid_argument 如果列数和值数不匹配。
         * @throws std::runtime_error 如果数据库连接无效或插入失败。
         */
        void insert(const std::string& tableName, const std::vector<std::string>& columns, const std::vector<std::vector<std::string>>& values) {
            std::lock_guard<std::recursive_mutex> lock(dbMutex_); // 线程安全锁
            if (!db_) {
                throw std::runtime_error("Database connection is closed.");
            }
            if (columns.empty() || values.empty()) {
                return; // 没有数据或列定义，直接返回
            }

            // 构建 SQL 语句的列名和占位符部分
            std::string columnsSql = "(";
            std::string placeholders = "(";
            for (size_t i = 0; i < columns.size(); ++i) {
                columnsSql += escapeIdentifier(columns[i]);
                placeholders += "?"; // 使用 '?' 作为参数占位符，这是防止 SQL 注入的关键
                if (i < columns.size() - 1) {
                    columnsSql += ", ";
                    placeholders += ", ";
                }
            }
            columnsSql += ")";
            placeholders += ")";
            std::string sql = "INSERT INTO " + escapeIdentifier(tableName) + " " + columnsSql + " VALUES " + placeholders + ";";

            sqlite3_stmt* stmt = nullptr; // 初始化为 nullptr
            // 预编译 SQL 语句
            if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
                throw std::runtime_error("预编译 'insert' 语句失败: " + std::string(sqlite3_errmsg(db_)));
            }

            // !!! 核心修改：移除此处对 Transaction 对象的创建和管理。
            //    事务管理现在完全由调用方负责，以避免事务嵌套错误。

            try {
                for (const auto& rowValues : values) {
                    if (rowValues.size() != columns.size()) {
                        throw std::invalid_argument("插入数据时，某行的值的数量与列的数量不匹配。");
                    }
                    // 绑定参数
                    for (size_t i = 0; i < rowValues.size(); ++i) {
                        // sqlite3_bind_text 的索引从 1 开始
                        sqlite3_bind_text(stmt, i + 1, rowValues[i].c_str(), -1, SQLITE_TRANSIENT);
                    }

                    // 执行语句。SQLITE_DONE 表示语句执行成功且没有更多结果行。
                    if (sqlite3_step(stmt) != SQLITE_DONE) {
                        throw std::runtime_error("执行插入失败: " + std::string(sqlite3_errmsg(db_)));
                    }
                    sqlite3_reset(stmt); // 重置语句，以便下一次循环可以重新绑定参数和执行
                    sqlite3_clear_bindings(stmt); // 清除上一次的参数绑定
                }
                // !!! 核心修改：移除此处对事务的提交操作。
            } catch (...) {
                // 如果在执行批量插入过程中发生任何错误，确保在异常发生时释放语句句柄。
                sqlite3_finalize(stmt);
                throw; // 重新抛出原始异常
            }

            sqlite3_finalize(stmt); // 完成所有插入后释放语句句柄
        }

        /**
         * @brief 删除数据。
         *
         * @param tableName 表名。
         * @param condition 删除条件，例如 "age < 30"。如果为空，则删除所有数据。
         * @throws std::runtime_error 如果数据库连接无效或删除失败。
         * @warning `condition` 参数是直接拼接到SQL语句中的，请确保其内容是安全的，以避免SQL注入。
         */
        void remove(const std::string& tableName, const std::string& condition = "") {
            std::lock_guard<std::recursive_mutex> lock(dbMutex_); // 线程安全锁
            if (!db_) {
                throw std::runtime_error("Database connection is closed.");
            }
            std::string sql = "DELETE FROM " + escapeIdentifier(tableName);
            if (!condition.empty()) {
                sql += " WHERE " + condition; // 条件字符串由用户提供，需自行确保安全性。
            }
            sql += ";";
            executeSQL(sql); // 执行 SQL 语句
        }

        /**
         * @brief 更新数据。使用预编译语句和参数绑定以保证安全。
         *
         * @param tableName 表名。
         * @param columns 要更新的列名列表。
         * @param values 对应的新值列表。
         * @param condition 更新条件，例如 "id = 5"。
         * @throws std::invalid_argument 如果列数和值数不匹配。
         * @throws std::runtime_error 如果数据库连接无效或更新失败。
         * @warning `condition` 参数是直接拼接到SQL语句中的，请确保其内容是安全的，以避免SQL注入。
         */
        void update(const std::string& tableName, const std::vector<std::string>& columns, const std::vector<std::string>& values, const std::string& condition) {
            std::lock_guard<std::recursive_mutex> lock(dbMutex_); // 线程安全锁
            if (!db_) {
                throw std::runtime_error("Database connection is closed.");
            }
            if (columns.empty() || values.empty()) {
                return; // 没有列或值需要更新，直接返回
            }
            if (columns.size() != values.size()) {
                throw std::invalid_argument("更新数据时，列的数量与值的数量必须匹配。");
            }

            // 构建 UPDATE 语句，使用 ? 占位符进行参数绑定。
            std::string sql = "UPDATE " + escapeIdentifier(tableName) + " SET ";
            for (size_t i = 0; i < columns.size(); ++i) {
                sql += escapeIdentifier(columns[i]) + " = ?";
                if (i < columns.size() - 1) {
                    sql += ", ";
                }
            }
            if (!condition.empty()) {
                sql += " WHERE " + condition; // 条件字符串由用户提供，需自行确保安全性。
            }
            sql += ";";

            sqlite3_stmt* stmt = nullptr; // 初始化为 nullptr
            // 预编译 SQL 语句
            if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
                throw std::runtime_error("预编译 'update' 语句失败: " + std::string(sqlite3_errmsg(db_)));
            }
            // 绑定参数
            for (size_t i = 0; i < values.size(); ++i) {
                // sqlite3_bind_text 的索引从 1 开始
                sqlite3_bind_text(stmt, i + 1, values[i].c_str(), -1, SQLITE_TRANSIENT);
            }

            // 执行语句
            if (sqlite3_step(stmt) != SQLITE_DONE) {
                std::string errMsg = "执行更新失败: " + std::string(sqlite3_errmsg(db_));
                sqlite3_finalize(stmt); // 确保在错误发生时释放语句句柄
                throw std::runtime_error(errMsg);
            }

            sqlite3_finalize(stmt); // 释放语句句柄
        }

        /**
         * @brief 查询数据。
         *
         * @param tableName 表名。
         * @param columns 要查询的列名列表。如果为空，则查询所有列（SELECT *）。
         * @param condition 查询条件，例如 "age > 25"。
         * @param orderBy 排序条件，例如 "name ASC, age DESC"。
         * @param limit 查询结果的最大行数。-1表示无限制。
         * @param offset 查询结果的偏移量。-1表示无偏移。
         * @return std::vector<std::vector<std::string>> 查询结果。每行是一个字符串向量。
         * @throws std::runtime_error 如果数据库连接无效或查询失败。
         * @warning `condition` 和 `orderBy` 参数是直接拼接到SQL语句中的，请确保其内容是安全的，以避免SQL注入。
         */
        std::vector<std::vector<std::string>> query(const std::string& tableName, const std::vector<std::string>& columns = {}, const std::string& condition = "", const std::string& orderBy = "", int limit = -1, int offset = -1) {
            std::lock_guard<std::recursive_mutex> lock(dbMutex_); // 线程安全锁
            if (!db_) {
                throw std::runtime_error("Database connection is closed.");
            }

            // 构建 SELECT 语句。
            std::string sql = "SELECT ";
            if (columns.empty()) {
                sql += "*"; // 查询所有列
            } else {
                for (size_t i = 0; i < columns.size(); ++i) {
                    sql += escapeIdentifier(columns[i]); // 转义列名
                    if (i < columns.size() - 1) {
                        sql += ", ";
                    }
                }
            }
            sql += " FROM " + escapeIdentifier(tableName); // 转义表名
            if (!condition.empty()) {
                sql += " WHERE " + condition; // 条件字符串由用户提供，需自行确保安全性。
            }
            if (!orderBy.empty()) {
                sql += " ORDER BY " + orderBy; // 排序条件由用户提供，需自行确保安全性。
            }
            if (limit >= 0) {
                sql += " LIMIT " + std::to_string(limit); // 限制返回行数
            }
            if (offset >= 0) {
                sql += " OFFSET " + std::to_string(offset); // 设置偏移量
            }
            sql += ";";

            return executeQuery(sql); // 执行查询并返回结果
        }

        /**
         * @class Transaction
         * @brief 事务上下文管理器类，用于通过RAII原则管理数据库事务。
         *
         * @details
         * 在构造时自动开始一个 **EXCLUSIVE** 事务（`BEGIN EXCLUSIVE TRANSACTION;`）。
         * 如果 `Transaction` 对象在 `commit()` 未被显式调用的情况下析构，
         * 则会自动回滚事务（`ROLLBACK;`）。
         * 这可以确保即使在发生异常或程序提前退出的情况下，事务也能被正确处理，防止数据不一致。
         */
        class Transaction {
        public:
            /**
             * @brief 构造函数，开始一个数据库事务。
             * @param db SQLiteDB对象的引用。
             * @throws std::runtime_error 如果数据库连接无效，无法开始事务。
             */
            explicit Transaction(SQLiteDB& db) : db_(db), committed_(false) {
                // 在事务构造函数中加锁，以确保事务开始操作的线程安全。
                // 尽管 db_.executeSQL() 内部也会加锁（递归锁），但这里的显式加锁增加了清晰度。
                std::lock_guard<std::recursive_mutex> lock(db_.dbMutex_);
                if (!db_.db_) {
                    throw std::runtime_error("Cannot begin transaction - database connection is closed.");
                }
                // 开始一个排他性事务。排他性事务会阻止其他连接进行读写，直到事务结束，提供最高隔离级别。
                db_.executeSQL("BEGIN EXCLUSIVE TRANSACTION;");
            }

            /**
             * @brief 析构函数，如果事务未提交则自动回滚。
             */
            ~Transaction() {
                // 在析构函数中加锁，确保回滚操作的线程安全。
                std::lock_guard<std::recursive_mutex> lock(db_.dbMutex_);
                // 只有当事务未被提交且数据库连接仍然有效时，才执行回滚。
                if (!committed_ && db_.db_) {
                    try {
                        db_.executeSQL("ROLLBACK;");
                    } catch (const std::exception& e) {
                        // 在析构函数中抛出异常是不安全的行为，因此只记录错误。
                        std::cerr << "事务自动回滚时发生错误: " << e.what() << std::endl;
                    }
                }
            }

            /**
             * @brief 提交事务。
             *
             * @details
             * 只有在事务尚未提交且数据库连接有效的情况下才会执行提交操作。
             * 成功提交后，将 `committed_` 标志设置为 true，防止重复提交或意外回滚。
             */
            void commit() {
                // 在提交操作中加锁，确保提交操作的线程安全。
                std::lock_guard<std::recursive_mutex> lock(db_.dbMutex_);
                if (!committed_ && db_.db_) {
                    db_.executeSQL("COMMIT;"); // 执行提交
                    committed_ = true;        // 标记事务已提交
                }
            }

            // 禁用拷贝构造函数和拷贝赋值操作符。
            // 事务对象不应被复制，每个事务应独立管理其生命周期。
            Transaction(const Transaction&) = delete;
            Transaction& operator=(const Transaction&) = delete;

        private:
            SQLiteDB& db_;    ///< 对所属 SQLiteDB 实例的引用
            bool committed_;  ///< 标记事务是否已提交
        };

    private:
        std::string dbName_;                     ///< 数据库文件名
        sqlite3* db_;                            ///< SQLite数据库连接句柄
        mutable std::recursive_mutex dbMutex_;   ///< 递归互斥锁，用于保证所有数据库操作的线程安全。
                                                 ///< 声明为 mutable 使其能在 const 成员函数（如 escapeString, escapeIdentifier）中被锁定，
                                                 ///< 但此处主要保护非 const 的数据库操作。

        /**
         * @brief 私有辅助函数：执行没有返回结果的 SQL 语句（例如 DDL、DML 操作）。
         *
         * @details
         * 此函数内部会获取 `dbMutex_` 锁，确保线程安全。
         *
         * @param sql 要执行的 SQL 语句字符串。
         * @throws std::runtime_error 如果数据库连接无效或 SQL 执行失败。
         */
        void executeSQL(const std::string& sql) {
            // 获取互斥锁，确保在执行 SQL 时只有一个线程可以访问数据库句柄。
            std::lock_guard<std::recursive_mutex> lock(dbMutex_);
            // 防御性检查：确保数据库连接在操作前是开放和有效的。
            if (!db_) {
                throw std::runtime_error("Database connection is closed. (SQL: " + sql + ")");
            }

            char* errMsg = nullptr; // 用于存储 SQLite 错误消息
            // 执行 SQL 语句。nullptr 表示不提供回调函数和回调参数。
            int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &errMsg);
            if (rc != SQLITE_OK) {
                std::string error = "SQL执行错误: " + std::string(errMsg ? errMsg : "未知错误") + " (SQL: " + sql + ")";
                sqlite3_free(errMsg); // 释放由 sqlite3_exec 分配的错误消息内存
                throw std::runtime_error(error);
            }
        }

        /**
         * @brief 私有辅助函数：执行有返回结果的查询 SQL 语句（SELECT 操作）。
         *
         * @details
         * 此函数内部会获取 `dbMutex_` 锁，确保线程安全。
         * 它会预编译 SQL 语句，逐步获取结果，并在完成或发生错误时正确终结语句。
         *
         * @param sql 查询 SQL 语句字符串。
         * @return std::vector<std::vector<std::string>> 查询结果，每行是一个字符串向量。
         * @throws std::runtime_error 如果数据库连接无效或查询失败。
         */
        std::vector<std::vector<std::string>> executeQuery(const std::string& sql) {
            // 获取互斥锁，确保在执行查询时只有一个线程可以访问数据库句柄。
            std::lock_guard<std::recursive_mutex> lock(dbMutex_);
            // 防御性检查：确保数据库连接在操作前是开放和有效的。
            if (!db_) {
                throw std::runtime_error("Database connection is closed. (SQL: " + sql + ")");
            }

            sqlite3_stmt* stmt = nullptr; // 初始化为 nullptr，以防 prepare 失败
            std::vector<std::vector<std::string>> result;

            try {
                // 预编译 SQL 语句
                if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
                    throw std::runtime_error("预编译查询失败: " +
                                             std::string(sqlite3_errmsg(db_)) + " (SQL: " + sql + ")");
                }

                // 获取查询结果的列数
                int colCount = sqlite3_column_count(stmt);
                // 逐行处理查询结果
                while (sqlite3_step(stmt) == SQLITE_ROW) {
                    std::vector<std::string> row;
                    row.reserve(colCount); // 预留空间，提高效率
                    // 遍历每一列，获取其文本内容
                    for (int i = 0; i < colCount; ++i) {
                        const char* columnText = reinterpret_cast<const char*>(sqlite3_column_text(stmt, i));
                        row.push_back(columnText ? columnText : ""); // 将 SQL 的 NULL 值转换为空字符串
                    }
                    result.push_back(row); // 将当前行添加到结果集中
                }

                sqlite3_finalize(stmt); // 释放预编译语句句柄
                return result;
            } catch (...) {
                // 捕获所有异常，确保在异常发生时也能正确释放语句句柄。
                if (stmt) sqlite3_finalize(stmt);
                throw; // 重新抛出原始异常
            }
        }

        /**
         * @brief 转义 SQL 字符串字面量，用于拼接值以防止 SQL 注入。
         *
         * @details
         * 将字符串中的所有单引号 `'` 替换为双单引号 `''`。
         * **推荐优先使用参数绑定（即 SQL 语句中的 `?` 占位符）**，此函数仅作为备用或在特殊情况下使用。
         *
         * @param str 要转义的原始字符串。
         * @return std::string 转义后的字符串，可安全地插入到 SQL 语句的单引号字符串中。
         */
        std::string escapeString(const std::string& str) const {
            std::string escapedStr;
            escapedStr.reserve(str.length() + str.length() / 10); // 预估转义后长度，减少重新分配
            for (char c : str) {
                if (c == '\'') {
                    escapedStr += "''"; // 双单引号转义
                } else {
                    escapedStr += c;
                }
            }
            return escapedStr;
        }

        /**
         * @brief 转义 SQL 标识符（如表名、列名），允许使用关键字或特殊字符作为名称。
         *
         * @details
         * 将标识符用双引号 `"` 包围，并将其中的双引号 `"` 转义为两个双引号 `""`。
         *
         * @param identifier 要转义的原始标识符。
         * @return std::string 转义后的标识符，已用双引号包围，可安全地用于 SQL 语句。
         */
        std::string escapeIdentifier(const std::string& identifier) const {
            std::string escapedIdentifier = "\""; // 以双引号开始
            escapedIdentifier.reserve(identifier.length() + identifier.length() / 10 + 2); // 预留空间
            for (char c : identifier) {
                if (c == '"') {
                    escapedIdentifier += "\"\""; // 双引号转义
                } else {
                    escapedIdentifier += c;
                }
            }
            escapedIdentifier += "\""; // 以双引号结束
            return escapedIdentifier;
        }
    };
} // namespace LSX_LIB

#endif // LSX_LIB_SQLITEDB_H