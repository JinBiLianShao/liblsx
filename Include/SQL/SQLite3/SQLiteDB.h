/**
 * @file SQLiteDB.h
 * @brief SQLite数据库操作类，封装了SQLite数据库的常用操作。
 * @author 连思鑫
 * @date 2025-1-22
 *
 * @details
 * 该类封装了SQLite数据库的常用操作，包括创建表、插入数据、删除数据、更新数据、查询数据等。
 * 支持事务管理，确保数据操作的原子性。
 *
 * @example
 * 以下是一个使用示例：
 * @code
 * int main() {
 *     try {
 *         SQLiteDB db("example.db");  // 打开或创建数据库文件 example.db
 *
 *         // 检查表是否存在，如果不存在则创建表
 *         if (!db.tableExists("users")) {
 *             std::cout << "Creating table 'users'..." << std::endl;
 *             db.createTable("users", {"id INTEGER PRIMARY KEY AUTOINCREMENT", "name TEXT NOT NULL", "age INTEGER"});
 *         } else {
 *             std::cout << "Table 'users' already exists." << std::endl;
 *         }
 *
 *         // 插入数据（支持批量插入）
 *         std::cout << "Inserting data into 'users'..." << std::endl;
 *         db.insert("users", {"name", "age"}, {{"Alice", "30"}, {"Bob", "25"}, {"Charlie", "22"}});
 *
 *         // 查询数据
 *         std::cout << "Querying data from 'users':" << std::endl;
 *         auto result = db.query("users", {"name", "age"}, "age > 25", "age DESC", 10, 0);
 *         for (const auto& row : result) {
 *             for (const auto& col : row) {
 *                 std::cout << col << " ";
 *             }
 *             std::cout << std::endl;
 *         }
 *
 *         // 更新数据
 *         std::cout << "Updating data in 'users'..." << std::endl;
 *         db.update("users", {"age"}, {"31"}, "name = 'Alice'");
 *
 *         // 删除数据
 *         std::cout << "Deleting data from 'users'..." << std::endl;
 *         db.remove("users", "name = 'Bob'");
 *
 *         // 使用事务批量插入数据
 *         std::cout << "Using transaction to insert more data..." << std::endl;
 *         {
 *             SQLiteDB::Transaction transaction(db);  // 开启事务
 *             db.insert("users", {"name", "age"}, {{"David", "28"}, {"Eve", "24"}});
 *             transaction.commit();  // 提交事务
 *         }
 *
 *         // 查询所有数据
 *         std::cout << "Querying all data from 'users':" << std::endl;
 *         result = db.query("users", {}, "", "", -1, -1);
 *         for (const auto& row : result) {
 *             for (const auto& col : row) {
 *                 std::cout << col << " ";
 *             }
 *             std::cout << std::endl;
 *         }
 *
 *     } catch (const std::exception& e) {
 *         std::cerr << "Error: " << e.what() << std::endl;
 *         return 1;
 *     }
 *
 *     std::cout << "Database operations completed successfully." << std::endl;
 *     return 0;
 * }
 * @endcode
 */
#pragma once
#include <sqlite3.h>
#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <stdexcept>

//添加命名空间，防止类冲突
namespace LSX_LIB::SQLiteDB
{
    /**
     * @class SQLiteDB
     * @brief SQLite数据库操作类，封装了SQLite数据库的常用操作。
     *
     * @details
     * 该类封装了SQLite数据库的常用操作，包括创建表、插入数据、删除数据、更新数据、查询数据等。
     * 支持事务管理，确保数据操作的原子性。
     */
    class SQLiteDB {
    public:
        /**
         * @brief 构造函数，打开或创建数据库。
         *
         * @param dbName 数据库文件名。
         * @throws std::runtime_error 如果打开数据库失败。
         */
        explicit SQLiteDB(const std::string& dbName) : dbName(dbName) {
            int rc = sqlite3_open(dbName.c_str(), &db);
            if (rc != SQLITE_OK) {
                throw std::runtime_error("Can't open database: " + std::string(sqlite3_errmsg(db)));
            }
        }

        /**
         * @brief 析构函数，关闭数据库。
         */
        ~SQLiteDB() {
            if (db) {
                sqlite3_close(db);
            }
        }

        /**
         * @brief 检查表是否存在。
         *
         * @param tableName 表名。
         * @return bool 如果表存在返回true，否则返回false。
         */
        bool tableExists(const std::string& tableName) {
            std::string sql = "SELECT name FROM sqlite_master WHERE type='table' AND name='" + escapeString(tableName) + "';";
            auto result = executeQuery(sql);
            return !result.empty();
        }

        /**
         * @brief 创建表。
         *
         * @param tableName 表名。
         * @param columns 表的列定义。
         */
        void createTable(const std::string& tableName, const std::vector<std::string>& columns) {
            if (tableExists(tableName)) {
                std::cerr << "Table already exists: " << tableName << std::endl;
                return;
            }

            std::string sql = "CREATE TABLE " + tableName + " (";
            for (size_t i = 0; i < columns.size(); ++i) {
                sql += columns[i];
                if (i < columns.size() - 1) {
                    sql += ", ";
                }
            }
            sql += ");";

            executeSQL(sql);
        }

        /**
         * @brief 插入数据（支持批量插入）。
         *
         * @param tableName 表名。
         * @param columns 列名。
         * @param values 要插入的值。
         * @throws std::invalid_argument 如果列数和值数不匹配。
         */
        void insert(const std::string& tableName, const std::vector<std::string>& columns, const std::vector<std::vector<std::string>>& values) {
            if (columns.size() != values[0].size()) {
                throw std::invalid_argument("Number of columns and values must match.");
            }

            std::string sql = "INSERT INTO " + tableName + " (";
            for (size_t i = 0; i < columns.size(); ++i) {
                sql += columns[i];
                if (i < columns.size() - 1) {
                    sql += ", ";
                }
            }
            sql += ") VALUES ";

            for (size_t i = 0; i < values.size(); ++i) {
                sql += "(";
                for (size_t j = 0; j < values[i].size(); ++j) {
                    sql += "'" + escapeString(values[i][j]) + "'";
                    if (j < values[i].size() - 1) {
                        sql += ", ";
                    }
                }
                sql += ")";
                if (i < values.size() - 1) {
                    sql += ", ";
                }
            }
            sql += ";";

            executeSQL(sql);
        }

        /**
         * @brief 删除数据。
         *
         * @param tableName 表名。
         * @param condition 删除条件。
         */
        void remove(const std::string& tableName, const std::string& condition = "") {
            std::string sql = "DELETE FROM " + tableName;
            if (!condition.empty()) {
                sql += " WHERE " + condition;
            }
            sql += ";";

            executeSQL(sql);
        }

        /**
         * @brief 更新数据。
         *
         * @param tableName 表名。
         * @param columns 列名。
         * @param values 要更新的值。
         * @param condition 更新条件。
         * @throws std::invalid_argument 如果列数和值数不匹配。
         */
        void update(const std::string& tableName, const std::vector<std::string>& columns, const std::vector<std::string>& values, const std::string& condition) {
            if (columns.size() != values.size()) {
                throw std::invalid_argument("Number of columns and values must match.");
            }

            std::string sql = "UPDATE " + tableName + " SET ";
            for (size_t i = 0; i < columns.size(); ++i) {
                sql += columns[i] + " = '" + escapeString(values[i]) + "'";
                if (i < columns.size() - 1) {
                    sql += ", ";
                }
            }
            if (!condition.empty()) {
                sql += " WHERE " + condition;
            }
            sql += ";";

            executeSQL(sql);
        }

        /**
         * @brief 查询数据。
         *
         * @param tableName 表名。
         * @param columns 要查询的列名。
         * @param condition 查询条件。
         * @param orderBy 排序条件。
         * @param limit 查询结果的最大行数。
         * @param offset 查询结果的偏移量。
         * @return std::vector<std::vector<std::string>> 查询结果。
         */
        std::vector<std::vector<std::string>> query(const std::string& tableName, const std::vector<std::string>& columns = {}, const std::string& condition = "", const std::string& orderBy = "", int limit = -1, int offset = -1) {
            std::string sql = "SELECT ";
            if (columns.empty()) {
                sql += "*";
            } else {
                for (size_t i = 0; i < columns.size(); ++i) {
                    sql += columns[i];
                    if (i < columns.size() - 1) {
                        sql += ", ";
                    }
                }
            }
            sql += " FROM " + tableName;
            if (!condition.empty()) {
                sql += " WHERE " + condition;
            }
            if (!orderBy.empty()) {
                sql += " ORDER BY " + orderBy;
            }
            if (limit > 0) {
                sql += " LIMIT " + std::to_string(limit);
            }
            if (offset > 0) {
                sql += " OFFSET " + std::to_string(offset);
            }
            sql += ";";

            return executeQuery(sql);
        }

        /**
         * @brief 开始事务。
         */
        void beginTransaction() {
            executeSQL("BEGIN TRANSACTION;");
        }

        /**
         * @brief 提交事务。
         */
        void commitTransaction() {
            executeSQL("COMMIT;");
        }

        /**
         * @brief 回滚事务。
         */
        void rollbackTransaction() {
            executeSQL("ROLLBACK;");
        }

        /**
         * @class Transaction
         * @brief 事务上下文管理器类，用于管理数据库事务。
         *
         * @details
         * 该类用于管理数据库事务，确保事务的原子性。
         */
        class Transaction {
        public:
            /**
             * @brief 构造函数，开始事务。
             *
             * @param db SQLiteDB对象引用。
             */
            explicit Transaction(SQLiteDB& db) : db(db), committed(false) {
                db.beginTransaction();
            }

            /**
             * @brief 析构函数，如果事务未提交则回滚。
             */
            ~Transaction() {
                if (!committed) {
                    db.rollbackTransaction();
                }
            }

            /**
             * @brief 提交事务。
             */
            void commit() {
                db.commitTransaction();
                committed = true;
            }

        private:
            SQLiteDB& db;
            bool committed;
        };

    private:
        std::string dbName; ///< 数据库文件名
        sqlite3* db; ///< SQLite数据库句柄

        /**
         * @brief 执行SQL语句的通用函数。
         *
         * @param sql SQL语句。
         * @throws std::runtime_error 如果执行SQL语句失败。
         */
        void executeSQL(const std::string& sql) {
            char* errMsg = nullptr;
            int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errMsg);
            if (rc != SQLITE_OK) {
                std::string error = "SQL error: " + std::string(errMsg);
                sqlite3_free(errMsg);
                throw std::runtime_error(error);
            }
        }

        /**
         * @brief 执行查询的通用函数。
         *
         * @param sql 查询SQL语句。
         * @return std::vector<std::vector<std::string>> 查询结果。
         * @throws std::runtime_error 如果查询失败。
         */
        std::vector<std::vector<std::string>> executeQuery(const std::string& sql) {
            std::vector<std::vector<std::string>> result;

            sqlite3_stmt* stmt;
            int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                throw std::runtime_error("Failed to prepare query: " + std::string(sqlite3_errmsg(db)));
            }

            while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
                std::vector<std::string> row;
                for (int i = 0; i < sqlite3_column_count(stmt); ++i) {
                    row.push_back(reinterpret_cast<const char*>(sqlite3_column_text(stmt, i)));
                }
                result.push_back(row);
            }

            if (rc != SQLITE_DONE) {
                sqlite3_finalize(stmt);
                throw std::runtime_error("Query failed: " + std::string(sqlite3_errmsg(db)));
            }

            sqlite3_finalize(stmt);
            return result;
        }

        /**
         * @brief 转义字符串，防止SQL注入。
         *
         * @param str 要转义的字符串。
         * @return std::string 转义后的字符串。
         */
        std::string escapeString(const std::string& str) {
            std::string result;
            for (char c : str) {
                if (c == '\'') {
                    result += "''";
                } else {
                    result += c;
                }
            }
            return result;
        }
    };
}