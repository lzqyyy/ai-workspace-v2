#include "database.hpp"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>

namespace ws {

struct PgPoolState {
    PgPoolState(std::string value, int maximum, int timeout_ms)
        : conninfo(std::move(value)), max_connections(maximum), acquire_timeout_ms(timeout_ms) {}

    ~PgPoolState()
    {
        for (PGconn* conn : idle) {
            if (conn) PQfinish(conn);
        }
    }

    std::string conninfo;
    int max_connections;
    int acquire_timeout_ms;
    int total{0};
    std::deque<PGconn*> idle;
    std::mutex mutex;
    std::condition_variable available;
};

namespace {

PGconn* open_connection(const std::string& conninfo)
{
    PGconn* conn = PQconnectdb(conninfo.c_str());
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
            pool->idle.push_back(conn_);
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

Database::Database(std::string conninfo, int max_connections, int acquire_timeout_ms)
    : conninfo_(std::move(conninfo))
{
    if (max_connections < 1)
        throw ServiceError(500, "INTERNAL", "database pool size must be at least 1", false);
    if (acquire_timeout_ms < 1)
        throw ServiceError(500, "INTERNAL", "database acquire timeout must be positive", false);
    pool_ = std::make_shared<PgPoolState>(conninfo_, max_connections, acquire_timeout_ms);
}

PgSession Database::session() const
{
    std::unique_lock lock(pool_->mutex);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(pool_->acquire_timeout_ms);
    while (true) {
        while (!pool_->idle.empty()) {
            PGconn* conn = pool_->idle.front();
            pool_->idle.pop_front();
            if (conn && PQstatus(conn) == CONNECTION_OK) {
                return PgSession(conn, pool_);
            }
            if (conn) PQfinish(conn);
            if (pool_->total > 0) --pool_->total;
        }
        if (pool_->total < pool_->max_connections) {
            ++pool_->total;
            lock.unlock();
            try {
                PGconn* conn = open_connection(conninfo_);
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
