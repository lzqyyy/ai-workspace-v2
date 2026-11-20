#pragma once

#include "common.hpp"
#include <libpq-fe.h>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace ws {

class PgResult {
public:
    PgResult() = default;
    explicit PgResult(PGresult* result) : result_(result, PQclear) {}
    PgResult(PgResult&&) noexcept = default;
    PgResult& operator=(PgResult&&) noexcept = default;
    PgResult(const PgResult&) = delete;
    PgResult& operator=(const PgResult&) = delete;

    bool ok() const { return result_ && (PQresultStatus(result_.get()) == PGRES_TUPLES_OK || PQresultStatus(result_.get()) == PGRES_COMMAND_OK); }
    int rows() const { return result_ ? PQntuples(result_.get()) : 0; }
    int columns() const { return result_ ? PQnfields(result_.get()) : 0; }
    bool is_null(int row, int column) const { return !result_ || PQgetisnull(result_.get(), row, column) != 0; }
    std::string get(int row, int column) const
    {
        if (!result_ || is_null(row, column)) return {};
        return std::string(PQgetvalue(result_.get(), row, column), static_cast<std::size_t>(PQgetlength(result_.get(), row, column)));
    }
    std::string error() const { return result_ ? PQresultErrorMessage(result_.get()) : "database result unavailable"; }
    PGresult* raw() const { return result_.get(); }

private:
    std::unique_ptr<PGresult, decltype(&PQclear)> result_{nullptr, PQclear};
};

struct PgPoolState;

class PgSession {
public:
    ~PgSession();
    PgSession(PgSession&& other) noexcept;
    PgSession& operator=(PgSession&& other) noexcept;
    PgSession(const PgSession&) = delete;
    PgSession& operator=(const PgSession&) = delete;

    PgResult query(const std::string& sql, const std::vector<std::optional<std::string>>& params = {});
    void begin();
    void commit();
    void rollback() noexcept;
    void exec_script(const std::string& sql);

private:
    explicit PgSession(PGconn* conn, std::shared_ptr<PgPoolState> pool);
    void release() noexcept;

    PGconn* conn_{nullptr};
    std::shared_ptr<PgPoolState> pool_;
    bool in_transaction_{false};
    bool reusable_{true};

    friend class Database;
};

class Database {
public:
    explicit Database(std::string conninfo, int max_connections = 8, int acquire_timeout_ms = 5000);
    PgSession session() const;
    bool healthy() const;
    void applyMigrations(const std::string& directory) const;
    const std::string& conninfo() const { return conninfo_; }

private:
    std::string conninfo_;
    std::shared_ptr<PgPoolState> pool_;
};

} // namespace ws
