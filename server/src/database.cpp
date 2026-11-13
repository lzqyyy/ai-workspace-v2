#include "database.hpp"
#include <cstring>
#include <filesystem>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <iterator>

namespace ws {

PgSession::PgSession(const std::string& conninfo)
{
    conn_ = PQconnectdb(conninfo.c_str());
    if (!conn_ || PQstatus(conn_) != CONNECTION_OK) {
        const std::string detail = conn_ ? PQerrorMessage(conn_) : "connection allocation failed";
        if (conn_) PQfinish(conn_);
        conn_ = nullptr;
        throw ServiceError(500, "INTERNAL", "database connection failed", true);
        (void)detail;
    }
    PQsetClientEncoding(conn_, "UTF8");
}

void PgSession::exec_script(const std::string& sql)
{
    PGresult* raw = PQexec(conn_, sql.c_str());
    const auto status = raw ? PQresultStatus(raw) : PGRES_FATAL_ERROR;
    const std::string detail = raw ? PQresultErrorMessage(raw) : "database execution failed";
    if (raw) PQclear(raw);
    if (status != PGRES_COMMAND_OK && status != PGRES_TUPLES_OK) {
        throw ServiceError(500, "INTERNAL", "database migration failed", true);
        (void)detail;
    }
}
PgSession::~PgSession()
{
    if (conn_) PQfinish(conn_);
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
        throw ServiceError(500, "INTERNAL", "database query failed", true);
    }
    return result;
}

void PgSession::begin() { (void)query("BEGIN"); }
void PgSession::commit() { (void)query("COMMIT"); }
void PgSession::rollback() noexcept
{
    if (!conn_) return;
    PGresult* r = PQexec(conn_, "ROLLBACK");
    if (r) PQclear(r);
}

Database::Database(std::string conninfo) : conninfo_(std::move(conninfo)) {}

PgSession Database::session() const { return PgSession(conninfo_); }

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
