/**
 * @file SQLiteDB.h
 * @brief SQLite数据库操作类，封装了SQLite数据库的常用功能。
 * @author 连思鑫
 * @date 2025-1-22 (Optimized: 2025-06-06)
 *
 * @details
 * 该类封装了SQLite数据库的常用操作，包括连接/关闭数据库、创建表、删除表、清空表、
 * 插入数据、删除数据、更新数据、查询数据、检查表是否存在以及获取列名。
 * 支持事务管理，确保数据操作的原子性。
 *
 * @example
 * 以下是一个详细的使用示例，展示了SQLiteDB类的主要功能和推荐用法：
 * @code
 * #include <iostream>
 * #include <vector>
 * #include <string>
 * #include <stdexcept>
 * #include "SQLiteDB.h" // 确保包含您的SQLiteDB头文件
 *
 * int main() {
 * try {
 * // 1. 数据库连接与初始化
 * // 尝试打开或创建一个名为 "example.db" 的SQLite数据库文件。
 * // 如果文件不存在，SQLite会自动创建它。如果打开失败，将抛出runtime_error。
 * LSX_LIB::SQLiteDB db("example.db");
 *
 * // 2. 表存在性检查与创建
 * // 使用 tableExists() 方法检查 "users" 表是否已经存在。
 * // 这是一个良好的实践，可以避免重复创建表导致错误或警告。
 * if (!db.tableExists("users")) {
 * std::cout << "Creating table 'users'..." << std::endl;
 * // createTable() 方法用于创建新表。
 * // 第一个参数是表名，第二个参数是列定义的vector。
 * // "id INTEGER PRIMARY KEY AUTOINCREMENT" 会自动为每条新记录生成唯一的ID。
 * db.createTable("users", {"id INTEGER PRIMARY KEY AUTOINCREMENT", "name TEXT NOT NULL", "age INTEGER"});
 * } else {
 * std::cout << "Table 'users' already exists." << std::endl;
 * }
 *
 * // 3. 插入数据（支持批量插入）
 * std::cout << "Inserting data into 'users'..." << std::endl;
 * // insert() 方法支持一次性插入多行数据。
 * // 第一个参数是表名。
 * // 第二个参数是列名的vector，指定要插入数据的列。
 * // 第三个参数是包含多行数据的vector of vectors，每个内部vector代表一行数据，其顺序需与列名vector一致。
 * db.insert("users", {"name", "age"}, {{"Alice", "30"}, {"Bob", "25"}, {"Charlie", "22"}});
 *
 * // 4. 查询数据
 * std::cout << "Querying data from 'users' with condition and order:" << std::endl;
 * // query() 方法是灵活的查询接口。
 * // 第一个参数是表名。
 * // 第二个参数是要查询的列名vector。如果为空，则查询所有列（SELECT *）。
 * // 第三个参数是WHERE子句的条件字符串（例如 "age > 25"）。如果为空，则无条件。
 * // 第四个参数是ORDER BY子句的排序条件（例如 "age DESC"）。如果为空，则无排序。
 * // 第五个参数是LIMIT限制返回的行数。-1表示无限制。
 * // 第六个参数是OFFSET跳过的行数。-1表示无偏移。
 * auto result = db.query("users", {"name", "age"}, "age > 25", "age DESC", 10, 0);
 * // 遍历并打印查询结果。
 * for (const auto& row : result) {
 * for (const auto& col : row) {
 * std::cout << col << " ";
 * }
 * std::cout << std::endl;
 * }
 *
 * // 5. 更新数据
 * std::cout << "Updating Alice's age to 31..." << std::endl;
 * // update() 方法用于修改现有数据。
 * // 第一个参数是表名。
 * // 第二个参数是要更新的列名vector。
 * // 第三个参数是对应的新值vector，顺序与列名vector一致。
 * // 注意：对于字符串值，如果您直接在SQL中拼接，需要确保在value中包含单引号并进行转义（escapeString已处理）。
 * // 第四个参数是WHERE子句的条件，指定哪些行将被更新。
 * db.update("users", {"age"}, {"31"}, "name = 'Alice'");
 *
 * // 6. 获取列名
 * std::cout << "Column names for 'users':" << std::endl;
 * // getColumnNames() 方法可以获取指定表的所有列名。
 * // 这对于构建动态查询或根据表结构处理数据非常有用。
 * std::vector<std::string> columnNames = db.getColumnNames("users");
 * for (const auto& colName : columnNames) {
 * std::cout << colName << " ";
 * }
 * std::cout << std::endl;
 *
 * // 7. 删除数据
 * std::cout << "Deleting user Bob..." << std::endl;
 * // remove() 方法用于删除数据。
 * // 第一个参数是表名。
 * // 第二个参数是WHERE子句的条件。如果为空，则删除表中所有数据（清空表）。
 * db.remove("users", "name = 'Bob'");
 *
 * // 8. 使用事务批量操作
 * std::cout << "Using transaction to insert more data..." << std::endl;
 * // Transaction 类提供了RAII（资源获取即初始化）风格的事务管理。
 * // 在构造函数中自动开始事务，在析构函数中如果未显式提交，则自动回滚。
 * {
 * LSX_LIB::SQLiteDB::Transaction transaction(db); // 开启事务
 * // 在事务中执行多个DML操作，它们将作为一个原子操作单元。
 * db.insert("users", {"name", "age"}, {{"David", "28"}, {"Eve", "24"}});
 * // transaction.commit(); // 如果不调用commit，当transaction对象超出作用域时会自动回滚
 * transaction.commit(); // 显式提交事务
 * } // transaction对象在此处析构，如果未调用commit()将自动回滚
 *
 * // 9. 查询所有数据以验证变更
 * std::cout << "Querying all data from 'users' after modifications:" << std::endl;
 * // 查询所有列和所有行。
 * result = db.query("users", {}, "", "", -1, -1);
 * for (const auto& row : result) {
 * for (const auto& col : row) {
 * std::cout << col << " ";
 * }
 * std::cout << std::endl;
 * }
 *
 * // 10. 清空表
 * std::cout << "Clearing table 'users'..." << std::endl;
 * // clearTable() 方法删除表中的所有行，并重置AUTOINCREMENT序列。
 * db.clearTable("users");
 * // 验证表是否为空。
 * result = db.query("users", {"COUNT(*)"}, "", "", -1, -1);
 * std::cout << "Rows in 'users' after clearing: " << result[0][0] << std::endl;
 *
 * // 11. 删除表
 * std::cout << "Dropping table 'users'..." << std::endl;
 * // dropTable() 方法删除整个表。如果表不存在，不会抛出错误。
 * db.dropTable("users");
 * // 验证表是否已被删除。
 * if (!db.tableExists("users")) {
 * std::cout << "Table 'users' dropped successfully." << std::endl;
 * }
 *
 * } catch (const std::exception& e) {
 * // 捕获并打印所有数据库操作中可能抛出的异常。
 * std::cerr << "Error: " << e.what() << std::endl;
 * return 1;
 * }
 *
 * std::cout << "Database operations completed successfully." << std::endl;
 * return 0;
 * }
 * @endcode
 */
#ifndef LSX_LIB_SQLITEDB_H
#define LSX_LIB_SQLITEDB_H

#include <sqlite3.h>
#include <iostream>
#include <string>
#include <vector>
#include <memory>   // For std::unique_ptr (though not strictly used for db handle due to sqlite3_close)
#include <stdexcept>
#include <map>      // Included for completeness, though not directly used by the original 'update' signature.

//添加命名空间，防止类冲突
namespace LSX_LIB
{
    /**
     * @class SQLiteDB
     * @brief SQLite数据库操作类，封装了SQLite数据库的常用操作。
     *
     * @details
     * 该类封装了SQLite数据库的常用操作，包括创建表、删除表、清空表、
     * 插入数据、删除数据、更新数据、查询数据、检查表是否存在以及获取列名。
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
        explicit SQLiteDB(const std::string& dbName) : dbName(dbName), db(nullptr) {
            int rc = sqlite3_open(dbName.c_str(), &db);
            if (rc != SQLITE_OK) {
                // Ensure db handle is closed on failure to avoid resource leaks
                std::string errMsg = "Can't open database: " + std::string(sqlite3_errmsg(db));
                if (db) sqlite3_close(db);
                throw std::runtime_error(errMsg);
            }
            std::cout << "Opened database successfully: " << dbName << std::endl;
        }

        /**
         * @brief 析构函数，关闭数据库。
         */
        ~SQLiteDB() {
            if (db) {
                sqlite3_close(db);
                std::cout << "Database connection closed." << std::endl;
            }
        }

        // 禁用拷贝构造和赋值操作符，防止意外的数据库句柄复制，这有助于避免双重释放等问题。
        SQLiteDB(const SQLiteDB&) = delete;
        SQLiteDB& operator=(const SQLiteDB&) = delete;


        /**
         * @brief 检查表是否存在。
         *
         * @param tableName 表名。
         * @return bool 如果表存在返回true，否则返回false。
         * @throws std::runtime_error 如果查询失败。
         */
        bool tableExists(const std::string& tableName) {
            // Using sqlite_master table to check for table existence, safest way.
            // Using escapeString for the table name literal to prevent SQL injection.
            std::string sql = "SELECT name FROM sqlite_master WHERE type='table' AND name='" + escapeString(tableName) + "';";
            auto result = executeQuery(sql);
            return !result.empty();
        }

        /**
         * @brief 创建表。如果表已存在，则不会重复创建。
         *
         * @param tableName 表名。
         * @param columns 表的列定义，例如 {"id INTEGER PRIMARY KEY AUTOINCREMENT", "name TEXT NOT NULL", "age INTEGER"}。
         */
        void createTable(const std::string& tableName, const std::vector<std::string>& columns) {
            // Re-check table existence inside to avoid redundant check if user calls this method directly.
            // Original code had a redundant check, removing it here for efficiency.
            // The CREATE TABLE IF NOT EXISTS syntax handles existence check efficiently at DB level.
            std::string sql = "CREATE TABLE IF NOT EXISTS " + escapeIdentifier(tableName) + " (";
            for (size_t i = 0; i < columns.size(); ++i) {
                sql += columns[i]; // Column definitions usually don't need escaping here unless they are dynamic and contain special chars.
                // Assuming basic SQL types and constraints are provided.
                if (i < columns.size() - 1) {
                    sql += ", ";
                }
            }
            sql += ");";

            executeSQL(sql);
        }

        /**
         * @brief 删除表。如果表不存在，此操作不会报错。
         *
         * @param tableName 要删除的表的名称。
         */
        void dropTable(const std::string& tableName) {
            std::string sql = "DROP TABLE IF EXISTS " + escapeIdentifier(tableName) + ";";
            executeSQL(sql);
        }

        /**
         * @brief 清空表（删除所有行）。
         * 同时会重置自增序列（如果有的话），使新的插入从1开始。
         *
         * @param tableName 要清空的表的名称。
         */
        void clearTable(const std::string& tableName) {
            std::string sql = "DELETE FROM " + escapeIdentifier(tableName) + ";";
            executeSQL(sql);
            // Reset AUTOINCREMENT sequence for the table. This is important if you want new insertions to start from 1.
            sql = "DELETE FROM sqlite_sequence WHERE name = '" + escapeString(tableName) + "';";
            executeSQL(sql);
        }

        /**
         * @brief 获取指定表的列名。
         * 通过PRAGMA table_info语句查询表的结构信息。
         *
         * @param tableName 要获取列名的表的名称。
         * @return std::vector<std::string> 包含列名的向量。
         * @throws std::runtime_error 如果无法获取列信息或表不存在。
         */
        std::vector<std::string> getColumnNames(const std::string& tableName) {
            std::vector<std::string> columnNames;
            // PRAGMA table_info(table_name) returns a result set with columns like cid, name, type, notnull, dflt_value, pk.
            // We are interested in the 'name' column (index 1).
            std::string sql = "PRAGMA table_info(" + escapeIdentifier(tableName) + ");";

            sqlite3_stmt* stmt;
            int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                throw std::runtime_error("Failed to prepare PRAGMA table_info statement for table '" + tableName + "': " + std::string(sqlite3_errmsg(db)));
            }

            while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
                // Column 1 of table_info result is the column name
                const char* colName = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
                if (colName) {
                    columnNames.push_back(colName);
                }
            }

            if (rc != SQLITE_DONE) {
                std::string error = "Failed to retrieve column names for table '" + tableName + "': " + std::string(sqlite3_errmsg(db));
                sqlite3_finalize(stmt);
                throw std::runtime_error(error);
            }

            sqlite3_finalize(stmt);
            return columnNames;
        }

        /**
         * @brief 插入数据（支持批量插入）。
         *
         * @param tableName 表名。
         * @param columns 列名。
         * @param values 要插入的值。
         * @throws std::invalid_argument 如果列数和值数不匹配。
         * @throws std::runtime_error 如果插入失败。
         */
        void insert(const std::string& tableName, const std::vector<std::string>& columns, const std::vector<std::vector<std::string>>& values) {
            if (columns.empty() || values.empty()) {
                return; // Nothing to insert
            }
            if (columns.size() != values[0].size()) {
                throw std::invalid_argument("Number of columns and values in first row must match.");
            }

            std::string columnsSql = "(";
            std::string placeholders = "(";
            for (size_t i = 0; i < columns.size(); ++i) {
                columnsSql += escapeIdentifier(columns[i]);
                placeholders += "?"; // Use '?' for parameter binding, much safer than string concatenation
                if (i < columns.size() - 1) {
                    columnsSql += ", ";
                    placeholders += ", ";
                }
            }
            columnsSql += ")";
            placeholders += ")";

            std::string sql = "INSERT INTO " + escapeIdentifier(tableName) + " " + columnsSql + " VALUES " + placeholders + ";";

            sqlite3_stmt* stmt;
            int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                throw std::runtime_error("Failed to prepare insert statement: " + std::string(sqlite3_errmsg(db)) + " (SQL: " + sql + ")");
            }

            // Use a transaction for batch inserts for better performance
            beginTransaction();
            try {
                for (const auto& rowValues : values) {
                    if (rowValues.size() != columns.size()) {
                        throw std::runtime_error("Number of values (" + std::to_string(rowValues.size()) +
                                                 ") does not match number of columns (" + std::to_string(columns.size()) +
                                                 ") for an insert row.");
                    }
                    for (int i = 0; i < rowValues.size(); ++i) {
                        // +1 because SQLite bind parameters are 1-indexed
                        // SQLITE_TRANSIENT tells SQLite to make a private copy of the string
                        sqlite3_bind_text(stmt, i + 1, rowValues[i].c_str(), -1, SQLITE_TRANSIENT);
                    }

                    rc = sqlite3_step(stmt);
                    if (rc != SQLITE_DONE) {
                        throw std::runtime_error("Insert failed: " + std::string(sqlite3_errmsg(db)) + " (Values example: " + (rowValues.empty() ? "" : rowValues[0]) + "...)"); // Add more context
                    }
                    sqlite3_reset(stmt); // Reset the statement for the next iteration without re-preparing
                }
                commitTransaction(); // Commit transaction on success
            } catch (const std::exception& e) { // Catch std::exception for consistency
                rollbackTransaction(); // Rollback on error
                sqlite3_finalize(stmt); // Ensure statement is finalized
                throw; // Re-throw the exception
            }

            sqlite3_finalize(stmt); // Finalize the statement after all operations
        }

        /**
         * @brief 删除数据。
         *
         * @param tableName 表名。
         * @param condition 删除条件，例如 "age < 30"。如果为空，则删除所有数据。
         * @throws std::runtime_error 如果删除失败。
         */
        void remove(const std::string& tableName, const std::string& condition = "") {
            std::string sql = "DELETE FROM " + escapeIdentifier(tableName);
            if (!condition.empty()) {
                sql += " WHERE " + condition; // Conditions are user-provided, assume responsibility for safety.
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
         * @throws std::runtime_error 如果更新失败。
         */
        void update(const std::string& tableName, const std::vector<std::string>& columns, const std::vector<std::string>& values, const std::string& condition) {
            if (columns.empty() || values.empty()) {
                return; // Nothing to update
            }
            if (columns.size() != values.size()) {
                throw std::invalid_argument("Number of columns and values must match for update.");
            }

            std::string sql = "UPDATE " + escapeIdentifier(tableName) + " SET ";
            for (size_t i = 0; i < columns.size(); ++i) {
                sql += escapeIdentifier(columns[i]) + " = '" + escapeString(values[i]) + "'"; // Values are escaped.
                if (i < columns.size() - 1) {
                    sql += ", ";
                }
            }
            if (!condition.empty()) {
                sql += " WHERE " + condition; // Conditions are user-provided, assume responsibility for safety.
            }
            sql += ";";

            executeSQL(sql);
        }

        /**
         * @brief 查询数据。
         *
         * @param tableName 表名。
         * @param columns 要查询的列名。如果为空，则查询所有列（SELECT *）。
         * @param condition 查询条件，例如 "age > 25"。
         * @param orderBy 排序条件，例如 "name ASC, age DESC"。
         * @param limit 查询结果的最大行数。-1表示无限制。
         * @param offset 查询结果的偏移量。-1表示无偏移。
         * @return std::vector<std::vector<std::string>> 查询结果。每行是一个字符串向量。
         * @throws std::runtime_error 如果查询失败。
         */
        std::vector<std::vector<std::string>> query(const std::string& tableName, const std::vector<std::string>& columns = {}, const std::string& condition = "", const std::string& orderBy = "", int limit = -1, int offset = -1) {
            std::string sql = "SELECT ";
            if (columns.empty()) {
                sql += "*";
            } else {
                for (size_t i = 0; i < columns.size(); ++i) {
                    sql += escapeIdentifier(columns[i]); // Escape column names in SELECT clause
                    if (i < columns.size() - 1) {
                        sql += ", ";
                    }
                }
            }
            sql += " FROM " + escapeIdentifier(tableName); // Escape table name
            if (!condition.empty()) {
                sql += " WHERE " + condition; // Conditions are user-provided, assume responsibility for safety.
            }
            if (!orderBy.empty()) {
                sql += " ORDER BY " + orderBy; // Order by is user-provided, assume responsibility for safety.
            }
            if (limit >= 0) { // Changed from > 0 to >= 0 to allow limit 0 (no rows)
                sql += " LIMIT " + std::to_string(limit);
            }
            if (offset >= 0) { // Changed from > 0 to >= 0 to allow offset 0 (no offset)
                sql += " OFFSET " + std::to_string(offset);
            }
            sql += ";";

            return executeQuery(sql);
        }

        /**
         * @brief 开始一个数据库事务。
         * @throws std::runtime_error 如果事务无法开始。
         */
        void beginTransaction() {
            executeSQL("BEGIN TRANSACTION;");
        }

        /**
         * @brief 提交当前数据库事务。
         * @throws std::runtime_error 如果事务无法提交。
         */
        void commitTransaction() {
            executeSQL("COMMIT;");
        }

        /**
         * @brief 回滚当前数据库事务。
         * @throws std::runtime_error 如果事务无法回滚。
         */
        void rollbackTransaction() {
            executeSQL("ROLLBACK;");
        }

        /**
         * @class Transaction
         * @brief 事务上下文管理器类，用于管理数据库事务。
         *
         * @details
         * 该类用于管理数据库事务，确保事务的原子性。使用RAII (Resource Acquisition Is Initialization)
         * 原则，在构造时开始事务，在析构时如果未显式提交则自动回滚。
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
                    try {
                        db.rollbackTransaction();
                    } catch (const std::exception& e) {
                        // Log or handle the rollback error if necessary, but don't re-throw from destructor.
                        std::cerr << "Error during transaction rollback (possibly due to nested transaction/no active transaction): " << e.what() << std::endl;
                    }
                }
            }

            /**
             * @brief 提交事务。
             * @throws std::runtime_error 如果提交失败。
             */
            void commit() {
                db.commitTransaction();
                committed = true;
            }

            // Disable copy constructor and assignment operator for Transaction
            Transaction(const Transaction&) = delete;
            Transaction& operator=(const Transaction&) = delete;

        private:
            SQLiteDB& db;
            bool committed;
        };

    private:
        std::string dbName; ///< 数据库文件名
        sqlite3* db; ///< SQLite数据库句柄

        /**
         * @brief 执行SQL语句的通用函数。
         * 主要用于非查询操作 (DDL, DML without results).
         *
         * @param sql SQL语句。
         * @throws std::runtime_error 如果执行SQL语句失败。
         */
        void executeSQL(const std::string& sql) {
            char* errMsg = nullptr;
            int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errMsg);
            if (rc != SQLITE_OK) {
                std::string error = "SQL error: " + std::string(errMsg) + " (SQL: " + sql + ")";
                sqlite3_free(errMsg); // Free the error message allocated by sqlite3_exec
                throw std::runtime_error(error);
            }
        }

        /**
         * @brief 执行查询的通用函数。
         * 主要用于SELECT操作。
         *
         * @param sql 查询SQL语句。
         * @return std::vector<std::vector<std::string>> 查询结果。
         * @throws std::runtime_error 如果查询失败。
         */
        std::vector<std::vector<std::string>> executeQuery(const std::string& sql) {
            std::vector<std::vector<std::string>> result;

            sqlite3_stmt* stmt;
            // -1 for the length of the SQL string means sqlite3_prepare_v2 will read until a null terminator
            int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                throw std::runtime_error("Failed to prepare query: " + std::string(sqlite3_errmsg(db)) + " (SQL: " + sql + ")");
            }

            while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
                std::vector<std::string> row;
                int colCount = sqlite3_column_count(stmt);
                row.reserve(colCount); // Pre-allocate space to avoid reallocations

                for (int i = 0; i < colCount; ++i) {
                    // sqlite3_column_text returns a pointer to a UTF-8 string or NULL if the value is NULL.
                    const char* columnText = reinterpret_cast<const char*>(sqlite3_column_text(stmt, i));
                    row.push_back(columnText ? columnText : ""); // Convert NULL to empty string for safety
                }
                result.push_back(row);
            }

            // Check if the query finished successfully (SQLITE_DONE) or with an error
            if (rc != SQLITE_DONE) {
                std::string error = "Query failed: " + std::string(sqlite3_errmsg(db)) + " (SQL: " + sql + ")";
                sqlite3_finalize(stmt); // Ensure statement is finalized even on error
                throw std::runtime_error(error);
            }

            sqlite3_finalize(stmt); // Finalize the statement
            return result;
        }

        /**
         * @brief 转义SQL字符串字面量（数据值），防止SQL注入。
         * 通过将所有单引号 ' 替换为双单引号 '' 来实现。
         *
         * @param str 要转义的字符串字面量。
         * @return std::string 转义后的字符串。
         */
        std::string escapeString(const std::string& str) {
            std::string escapedStr;
            escapedStr.reserve(str.length() + str.length() / 10); // Pre-allocate with some room for expansion

            for (char c : str) {
                if (c == '\'') {
                    escapedStr += "''"; // Double single quotes
                } else {
                    escapedStr += c;
                }
            }
            return escapedStr;
        }

        /**
         * @brief 转义SQL标识符（表名、列名）。
         * 在SQLite中，通常通过双引号 " 来引用标识符，以允许使用关键字作为名称或包含特殊字符。
         * 此函数确保标识符内部的双引号 " 也被双引号 "" 转义。
         *
         * @param identifier 要转义的标识符。
         * @return std::string 转义后的标识符（包含双引号）。
         */
        std::string escapeIdentifier(const std::string& identifier) {
            std::string escapedIdentifier = "\""; // Start with opening double quote
            for (char c : identifier) {
                if (c == '"') {
                    escapedIdentifier += "\"\""; // Double double quotes
                } else {
                    escapedIdentifier += c;
                }
            }
            escapedIdentifier += "\""; // End with closing double quote
            return escapedIdentifier;
        }
    };
} // namespace LSX_LIB
#endif // LSX_LIB_SQLITEDB_H