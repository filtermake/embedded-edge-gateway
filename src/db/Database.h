#pragma once
#include <sqlite3.h>
#include <mutex>
#include <stdexcept>
#include <string>
#include <cstdio>
#include "Statement.h"

namespace gateway {

class Database {
public:
    explicit Database(const std::string& path) {
        int rc = sqlite3_open(path.c_str(), &db_);
        if (rc != SQLITE_OK) {
            std::string msg = db_ ? sqlite3_errmsg(db_) : "out of memory";
            if (db_) sqlite3_close(db_);
            db_ = nullptr;
            throw std::runtime_error("sqlite3_open failed: " + msg);
        }
        execNoThrow("PRAGMA journal_mode=WAL;");
        execNoThrow("PRAGMA synchronous=NORMAL;");

        const char* ddl =
            "CREATE TABLE IF NOT EXISTS device_data("
            " id        INTEGER PRIMARY KEY AUTOINCREMENT,"
            " device_id TEXT    NOT NULL,"
            " value     REAL,"
            " ts        INTEGER NOT NULL);";
        char* err = nullptr;
        if (sqlite3_exec(db_, ddl, nullptr, nullptr, &err) != SQLITE_OK) {
            std::string msg = err ? err : "?";
            sqlite3_free(err);
            sqlite3_close(db_);
            db_ = nullptr;
            throw std::runtime_error("create table failed: " + msg);
        }

        // 缓存 insert 语句:必须在建表之后 prepare(表不存在 prepare 会失败)
        insertStmt_ = new Statement(db_,
            "INSERT INTO device_data(device_id, value, ts) VALUES(?, ?, ?);");
    }

    ~Database() noexcept {
        delete insertStmt_;          // 先 finalize 语句,再 close 连接(顺序要紧)
        if (db_) sqlite3_close(db_);
    }

    Database(const Database&)            = delete;
    Database& operator=(const Database&) = delete;

    void insert(const std::string& device_id, double value, long ts) {
        std::lock_guard<std::mutex> lock(mtx_);
        insertStmt_->bind(1, device_id);
        insertStmt_->bind(2, value);
        insertStmt_->bind(3, ts);
        if (insertStmt_->step() != SQLITE_DONE) {
            fprintf(stderr, "[db] insert step failed: %s\n", sqlite3_errmsg(db_));
        }
        insertStmt_->reset();
    }

    long count() {
        std::lock_guard<std::mutex> lock(mtx_);
        Statement st(db_, "SELECT COUNT(*) FROM device_data;");
        return (st.step() == SQLITE_ROW) ? st.column_int64(0) : -1;
    }

private:
    void execNoThrow(const char* sql) {
        char* err = nullptr;
        if (sqlite3_exec(db_, sql, nullptr, nullptr, &err) != SQLITE_OK) {
            fprintf(stderr, "[db] pragma warn: %s\n", err ? err : "?");
            sqlite3_free(err);
        }
    }

    sqlite3*   db_         = nullptr;   // 拥有,负责 close
    Statement* insertStmt_ = nullptr;   // 拥有,缓存的 insert 语句
    std::mutex mtx_;                    // 保护 db_ 和 insertStmt_
};

} // namespace gateway
