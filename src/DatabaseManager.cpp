#include "DatabaseManager.h"

#ifdef _WIN32
#include <winsock2.h>  // some MariaDB builds expect this before mysql.h
#endif
#include <mysql.h>

#include <memory>
#include <type_traits>
#include <utility>

namespace {

// The connector's boolean type differs between versions (char vs bool),
// so we take it from the struct itself instead of guessing.
using DbBool = std::remove_pointer<decltype(MYSQL_BIND::is_null)>::type;

// Every result column is read into a buffer this big. Our widest column
// (VARCHAR(255) in utf8mb4) needs at most 1020 bytes.
const std::size_t kResultBufferSize = 1024;

// Closes a prepared statement automatically (RAII for MYSQL_STMT)
struct StmtDeleter {
    void operator()(MYSQL_STMT* stmt) const {
        if (stmt != nullptr) {
            mysql_stmt_close(stmt);
        }
    }
};
using StmtPtr = std::unique_ptr<MYSQL_STMT, StmtDeleter>;

std::string stmtError(MYSQL_STMT* stmt) {
    return "MySQL error " + std::to_string(mysql_stmt_errno(stmt)) + ": " +
           mysql_stmt_error(stmt);
}

// Prepares the SQL, binds the parameters, executes it, returns the statement.
StmtPtr runPrepared(MYSQL* conn, const std::string& sql,
                    const std::vector<std::string>& params) {
    StmtPtr stmt(mysql_stmt_init(conn));
    if (!stmt) {
        throw DatabaseException("mysql_stmt_init failed (out of memory)");
    }

    if (mysql_stmt_prepare(stmt.get(), sql.c_str(),
                           static_cast<unsigned long>(sql.size())) != 0) {
        throw DatabaseException("Prepare failed: " + stmtError(stmt.get()));
    }

    if (mysql_stmt_param_count(stmt.get()) != params.size()) {
        throw DatabaseException("Parameter count mismatch: SQL expects " +
                                std::to_string(mysql_stmt_param_count(stmt.get())) +
                                " but " + std::to_string(params.size()) + " given");
    }

    // A vector of a plain C struct starts zero-initialised, which is what we need.
    std::vector<MYSQL_BIND> binds(params.size());
    std::vector<unsigned long> lengths(params.size());
    for (std::size_t i = 0; i < params.size(); ++i) {
        lengths[i] = static_cast<unsigned long>(params[i].size());
        binds[i].buffer_type = MYSQL_TYPE_STRING;
        binds[i].buffer = const_cast<char*>(params[i].c_str());
        binds[i].buffer_length = lengths[i];
        binds[i].length = &lengths[i];
    }
    if (!params.empty() && mysql_stmt_bind_param(stmt.get(), binds.data()) != 0) {
        throw DatabaseException("Bind failed: " + stmtError(stmt.get()));
    }

    if (mysql_stmt_execute(stmt.get()) != 0) {
        throw DatabaseException("Execute failed: " + stmtError(stmt.get()));
    }
    return stmt;
}

}  // namespace

DatabaseManager::DatabaseManager(const DbConfig& config)
    : conn_(nullptr), inTransaction_(false) {
    conn_ = mysql_init(nullptr);
    if (conn_ == nullptr) {
        throw DatabaseException("mysql_init failed (out of memory)");
    }

    mysql_options(conn_, MYSQL_SET_CHARSET_NAME, "utf8mb4");
    unsigned int timeoutSeconds = 5;
    mysql_options(conn_, MYSQL_OPT_CONNECT_TIMEOUT, &timeoutSeconds);

    if (mysql_real_connect(conn_, config.host.c_str(), config.user.c_str(),
                           config.password.c_str(), config.database.c_str(),
                           config.port, nullptr, 0) == nullptr) {
        // If a constructor throws, the destructor does NOT run,
        // so we must release the handle ourselves.
        std::string message = "Connection failed: MySQL error " +
                              std::to_string(mysql_errno(conn_)) + ": " +
                              mysql_error(conn_);
        mysql_close(conn_);
        conn_ = nullptr;
        throw DatabaseException(message);
    }
}

DatabaseManager::~DatabaseManager() {
    if (conn_ != nullptr) {
        if (inTransaction_) {
            mysql_query(conn_, "ROLLBACK");  // never leave a transaction half-done
        }
        mysql_close(conn_);
    }
}

ResultSet DatabaseManager::query(const std::string& sql,
                                 const std::vector<std::string>& params) {
    StmtPtr stmt = runPrepared(conn_, sql, params);

    ResultSet rows;
    unsigned int columnCount = mysql_stmt_field_count(stmt.get());
    if (columnCount == 0) {
        return rows;  // statement produced no result set
    }

    // One text buffer per column; the library writes each fetched value into it.
    std::vector<std::vector<char>> buffers(columnCount,
                                           std::vector<char>(kResultBufferSize));
    std::vector<MYSQL_BIND> binds(columnCount);
    std::vector<unsigned long> lengths(columnCount, 0);
    // (not vector<bool>: that type can't give us element addresses)
    std::unique_ptr<DbBool[]> isNull = std::make_unique<DbBool[]>(columnCount);
    std::unique_ptr<DbBool[]> hasError = std::make_unique<DbBool[]>(columnCount);

    for (unsigned int i = 0; i < columnCount; ++i) {
        binds[i].buffer_type = MYSQL_TYPE_STRING;
        binds[i].buffer = buffers[i].data();
        binds[i].buffer_length = kResultBufferSize;
        binds[i].length = &lengths[i];
        binds[i].is_null = &isNull[i];
        binds[i].error = &hasError[i];
    }

    if (mysql_stmt_bind_result(stmt.get(), binds.data()) != 0) {
        throw DatabaseException("Bind result failed: " + stmtError(stmt.get()));
    }
    if (mysql_stmt_store_result(stmt.get()) != 0) {
        throw DatabaseException("Store result failed: " + stmtError(stmt.get()));
    }

    while (true) {
        int rc = mysql_stmt_fetch(stmt.get());
        if (rc == MYSQL_NO_DATA) {
            break;  // no more rows
        }
        if (rc == 1) {
            throw DatabaseException("Fetch failed: " + stmtError(stmt.get()));
        }
        if (rc == MYSQL_DATA_TRUNCATED) {
            throw DatabaseException("A column value was too large for the result buffer");
        }

        Row row;
        row.reserve(columnCount);
        for (unsigned int i = 0; i < columnCount; ++i) {
            if (isNull[i]) {
                row.emplace_back();  // SQL NULL -> empty string
            } else if (lengths[i] > kResultBufferSize) {
                throw DatabaseException("A column value was too large for the result buffer");
            } else {
                row.emplace_back(buffers[i].data(), lengths[i]);
            }
        }
        rows.push_back(std::move(row));
    }
    return rows;
}

unsigned long long DatabaseManager::execute(const std::string& sql,
                                            const std::vector<std::string>& params) {
    StmtPtr stmt = runPrepared(conn_, sql, params);
    return mysql_stmt_affected_rows(stmt.get());
}

unsigned long long DatabaseManager::lastInsertId() const {
    return mysql_insert_id(conn_);
}

void DatabaseManager::runPlain(const std::string& sql) {
    if (mysql_real_query(conn_, sql.c_str(),
                         static_cast<unsigned long>(sql.size())) != 0) {
        throw DatabaseException("MySQL error " + std::to_string(mysql_errno(conn_)) +
                                ": " + mysql_error(conn_));
    }
}

void DatabaseManager::beginTransaction() {
    if (inTransaction_) {
        throw DatabaseException("A transaction is already in progress");
    }
    runPlain("START TRANSACTION");
    inTransaction_ = true;
}

void DatabaseManager::commit() {
    if (!inTransaction_) {
        throw DatabaseException("No transaction in progress to commit");
    }
    runPlain("COMMIT");
    inTransaction_ = false;
}

void DatabaseManager::rollback() {
    if (!inTransaction_) {
        throw DatabaseException("No transaction in progress to roll back");
    }
    runPlain("ROLLBACK");
    inTransaction_ = false;
}

void DatabaseManager::rollbackQuietly() noexcept {
    if (conn_ == nullptr || !inTransaction_) {
        return;
    }
    (void)mysql_query(conn_, "ROLLBACK");  // result ignored on purpose
    inTransaction_ = false;                // whatever happened, we are no longer in one
}