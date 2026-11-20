#include "database.hpp"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>

namespace ws {

struct PooledConnection {
    PGconn* conn{nullptr};
    std::chrono::steady_clock::time_point idle_since{std::chrono::steady_clock::now()};
};

struct PgPoolState {
    PgPoolState(std::string value, int maximum, int timeout_ms, int connect_timeout, int idle_validate)
        : conninfo(std::move(value)), max_connections(maximum), acquire_timeout_ms(timeout_ms),
          connect_timeout_seconds(connect_timeout), idle_validate_seconds(idle_validate) {}

    ~PgPoolState()
    {
        for (const auto& slot : idle) {
            if (slot.conn) PQfinish(slot.conn);
        }
    }

    std::string conninfo;
    int max_connections;
    int acquire_timeout_ms;
    int connect_timeout_seconds;
    int idle_validate_seconds;
    int total{0};
    std::deque<PooledConnection> idle;
    std::mutex mutex;
    std::condition_variable available;
};

namespace {

PGconn* open_connection(const std::string& conninfo, int connect_timeout_seconds)
{
    char* parse_error = nullptr;
    PQconninfoOption* options = PQconninfoParse(conninfo.c_str(), &parse_error);
    if (!options) {
        if (parse_error) PQfreemem(parse_error);
        throw ServiceError(500, "INTERNAL", "database connection string is invalid", false);
    }

    std::vector<std::string> keywords;
    std::vector<std::string> values;
    bool has_connect_timeout = false;
    for (PQconninfoOption* option = options; option->keyword != nullptr; ++option) {
        if (std::strcmp(option->keyword, "connect_timeout") == 0) {
            has_connect_timeout = true;
            keywords.emplace_back(option->keyword);
            values.emplace_back(std::to_string(connect_timeout_seconds));
            continue;
        }
        if (!option->val) continue;
        keywords.emplace_back(option->keyword);
        values.emplace_back(option->val);
    }
    PQconninfoFree(options);
    if (!has_connect_timeout) {
        keywords.emplace_back("connect_timeout");
        values.emplace_back(std::to_string(connect_timeout_seconds));
    }
    if (parse_error) PQfreemem(parse_error);

    std::vector<const char*> keyword_ptrs;
    std::vector<const char*> value_ptrs;
    keyword_ptrs.reserve(keywords.size() + 1);
    value_ptrs.reserve(values.size() + 1);
    for (std::size_t i = 0; i < keywords.size(); ++i) {
        keyword_ptrs.push_back(keywords[i].c_str());
        value_ptrs.push_back(values[i].c_str());
    }
    keyword_ptrs.push_back(nullptr);
    value_ptrs.push_back(nullptr);
    PGconn* conn = PQconnectdbParams(keyword_ptrs.data(), value_ptrs.data(), 0);
    if (!conn || PQstatus(conn) != CONNECTION_OK) {
        const std::string detail = conn ? PQerrorMessage(conn) : "connection allocation failed";
        if (conn) PQfinish(conn);
        (void)detail;
        throw ServiceError(500, "INTERNAL", "database connection failed", true);
    }
    if (PQsetClientEncoding(conn, "UTF8") != 0) {
        PQfinish(conn);
        throw ServiceError(500, "INTERNAL", "database connection encoding failed", true);
    }
    PGresult* result = PQexec(conn, "SET application_name = 'ai-workspace-core'");
    const bool ok = result && PQresultStatus(result) == PGRES_COMMAND_OK;
    if (result) PQclear(result);
    if (!ok || PQstatus(conn) != CONNECTION_OK) {
        PQfinish(conn);
        throw ServiceError(500, "INTERNAL", "database connection setup failed", true);
    }
    return conn;
}

bool validate_connection(PGconn* conn)
{
    if (!conn || PQstatus(conn) != CONNECTION_OK) return false;
    PGresult* result = PQexec(conn, "SELECT 1");
    const bool ok = result && PQresultStatus(result) == PGRES_TUPLES_OK;
    if (result) PQclear(result);
    return ok && PQstatus(conn) == CONNECTION_OK;
}

} // namespace

PgSession::PgSession(PGconn* conn, std::shared_ptr<PgPoolState> pool)
    : conn_(conn), pool_(std::move(pool)) {}

PgSession::~PgSession()
{
    release();
}

PgSession::PgSession(PgSession&& other) noexcept
    : conn_(other.conn_), pool_(std::move(other.pool_)),
      in_transaction_(other.in_transaction_), reusable_(other.reusable_)
{
    other.conn_ = nullptr;
    other.in_transaction_ = false;
    other.reusable_ = true;
}

PgSession& PgSession::operator=(PgSession&& other) noexcept
{
    if (this != &other) {
        release();
        conn_ = other.conn_;
        pool_ = std::move(other.pool_);
        in_transaction_ = other.in_transaction_;
        reusable_ = other.reusable_;
        other.conn_ = nullptr;
        other.in_transaction_ = false;
        other.reusable_ = true;
    }
    return *this;
}

void PgSession::release() noexcept
{
    if (!conn_) {
        pool_.reset();
        return;
    }
    if (in_transaction_) rollback();
    const bool healthy = reusable_ && conn_ && PQstatus(conn_) == CONNECTION_OK;
    auto pool = std::move(pool_);
    if (healthy && pool) {
        {
            std::lock_guard lock(pool->mutex);
            pool->idle.push_back({conn_, std::chrono::steady_clock::now()});
        }
        pool->available.notify_one();
    } else {
        PQfinish(conn_);
        if (pool) {
            {
                std::lock_guard lock(pool->mutex);
                if (pool->total > 0) --pool->total;
            }
            pool->available.notify_one();
        }
    }
    conn_ = nullptr;
    in_transaction_ = false;
    reusable_ = true;
}

void PgSession::exec_script(const std::string& sql)
{
    PGresult* raw = PQexec(conn_, sql.c_str());
    const auto status = raw ? PQresultStatus(raw) : PGRES_FATAL_ERROR;
    const std::string detail = raw ? PQresultErrorMessage(raw) : "database execution failed";
    if (raw) PQclear(raw);
    if (status != PGRES_COMMAND_OK && status != PGRES_TUPLES_OK) {
        reusable_ = false;
        (void)detail;
        throw ServiceError(500, "INTERNAL", "database migration failed", true);
    }
}

PgResult PgSession::query(const std::string& sql, const std::vector<std::optional<std::string>>& params)
{
    std::vector<const char*> values(params.size(), nullptr);
    for (std::size_t i = 0; i < params.size(); ++i) {
        if (params[i]) values[i] = params[i]->c_str();
    }
    PgResult result(PQexecParams(conn_, sql.c_str(), static_cast<int>(params.size()), nullptr,
                                 values.data(), nullptr, nullptr, 0));
    if (!result.ok()) {
        reusable_ = false;
        throw ServiceError(500, "INTERNAL", "database query failed", true);
    }
    return result;
}

void PgSession::begin()
{
    (void)query("BEGIN");
    in_transaction_ = true;
}

void PgSession::commit()
{
    (void)query("COMMIT");
    in_transaction_ = false;
}

void PgSession::rollback() noexcept
{
    if (!conn_) return;
    PGresult* result = PQexec(conn_, "ROLLBACK");
    const bool ok = result && PQresultStatus(result) == PGRES_COMMAND_OK;
    if (result) PQclear(result);
    if (!ok) reusable_ = false;
    in_transaction_ = false;
}

Database::Database(std::string conninfo, int max_connections, int acquire_timeout_ms,
                   int connect_timeout_seconds, int idle_validate_seconds)
    : conninfo_(std::move(conninfo))
{
    if (max_connections < 1)
        throw ServiceError(500, "INTERNAL", "database pool size must be at least 1", false);
    if (acquire_timeout_ms < 1)
        throw ServiceError(500, "INTERNAL", "database acquire timeout must be positive", false);
    if (connect_timeout_seconds < 1)
        throw ServiceError(500, "INTERNAL", "database connect timeout must be positive", false);
    if (idle_validate_seconds < 0)
        throw ServiceError(500, "INTERNAL", "database idle validation interval must not be negative", false);
    pool_ = std::make_shared<PgPoolState>(conninfo_, max_connections, acquire_timeout_ms,
                                           connect_timeout_seconds, idle_validate_seconds);
}

PgSession Database::session() const
{
    std::unique_lock lock(pool_->mutex);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(pool_->acquire_timeout_ms);
    while (true) {
        std::optional<PooledConnection> candidate;
        if (!pool_->idle.empty()) {
            candidate = pool_->idle.front();
            pool_->idle.pop_front();
        }
        if (candidate) {
            lock.unlock();
            const auto now = std::chrono::steady_clock::now();
            const bool validate = candidate->conn &&
                (pool_->idle_validate_seconds == 0 ||
                 now - candidate->idle_since >= std::chrono::seconds(pool_->idle_validate_seconds));
            const bool usable = candidate->conn && PQstatus(candidate->conn) == CONNECTION_OK &&
                (!validate || validate_connection(candidate->conn));
            if (!usable && candidate->conn) PQfinish(candidate->conn);
            lock.lock();
            if (usable) return PgSession(candidate->conn, pool_);
            if (pool_->total > 0) --pool_->total;
            pool_->available.notify_one();
            continue;
        }
        if (pool_->total < pool_->max_connections) {
            ++pool_->total;
            lock.unlock();
            try {
                PGconn* conn = open_connection(conninfo_, pool_->connect_timeout_seconds);
                lock.lock();
                return PgSession(conn, pool_);
            } catch (...) {
                lock.lock();
                if (pool_->total > 0) --pool_->total;
                pool_->available.notify_one();
                throw;
            }
        }
        if (!pool_->available.wait_until(lock, deadline, [&]() {
                return !pool_->idle.empty() || pool_->total < pool_->max_connections;
            })) {
            throw ServiceError(500, "INTERNAL", "database connection pool exhausted", true);
        }
    }
}

void Database::applyMigrations(const std::string& directory) const
{
    namespace fs = std::filesystem;
    std::vector<fs::path> files;
    if (!fs::exists(directory)) throw ServiceError(500, "INTERNAL", "migrations directory is missing", true);
    for (const auto& entry : fs::directory_iterator(directory)) {
        if (entry.is_regular_file() && entry.path().extension() == ".sql") files.push_back(entry.path());
    }
    std::sort(files.begin(), files.end());
    auto session = this->session();
    for (const auto& file : files) {
        std::ifstream input(file, std::ios::binary);
        if (!input) throw ServiceError(500, "INTERNAL", "migration file is unreadable", true);
        const std::string sql((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        try {
            session.exec_script(sql);
        } catch (const ServiceError& e) {
            throw ServiceError(e.http_status, e.code, std::string(e.what()) + ": " + file.filename().string(), e.retryable);
        }
    }
}

bool Database::healthy() const
{
    try {
        auto session = this->session();
        auto result = session.query("SELECT 1");
        return result.rows() == 1;
    } catch (...) {
        return false;
    }
}

} // namespace ws
