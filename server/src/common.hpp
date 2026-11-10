#pragma once

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <iomanip>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace ws {

using Json = nlohmann::json;

struct Config {
    std::string database_url{"host=127.0.0.1 port=5432 dbname=workspace user=workspace password=workspace"};
    std::string http_host{"0.0.0.0"};
    int http_port{8080};
    std::string grpc_host{"0.0.0.0"};
    int grpc_port{50051};
    std::string frontend_dir{"frontend"};
    std::string migrations_dir{"db/migrations"};
    std::string app_origin{"http://localhost:8080"};
    int session_ttl_seconds{604800};
    bool cookie_secure{false};

    static Config load()
    {
        Config c;
        auto env = [](const char* name) -> std::optional<std::string> {
            const char* raw = std::getenv(name);
            if (raw == nullptr || *raw == '\0') return std::nullopt;
            return std::string(raw);
        };
        if (auto v = env("DATABASE_URL")) c.database_url = *v;
        if (auto v = env("HTTP_HOST")) c.http_host = *v;
        if (auto v = env("HTTP_PORT")) c.http_port = std::stoi(*v);
        if (auto v = env("GRPC_HOST")) c.grpc_host = *v;
        if (auto v = env("GRPC_PORT")) c.grpc_port = std::stoi(*v);
        if (auto v = env("FRONTEND_DIR")) c.frontend_dir = *v;
        if (auto v = env("MIGRATIONS_DIR")) c.migrations_dir = *v;
        if (auto v = env("APP_ORIGIN")) c.app_origin = *v;
        if (auto v = env("SESSION_TTL_SECONDS")) c.session_ttl_seconds = std::stoi(*v);
        if (auto v = env("COOKIE_SECURE")) {
            std::transform(v->begin(), v->end(), v->begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
            c.cookie_secure = (*v == "1" || *v == "true" || *v == "yes");
        }
        return c;
    }
};

class ServiceError : public std::runtime_error {
public:
    ServiceError(int http_status, std::string code, std::string message, bool retryable = false)
        : std::runtime_error(message), http_status(http_status), code(std::move(code)), retryable(retryable) {}

    int http_status;
    std::string code;
    bool retryable;
};

inline std::string lower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return value;
}

inline std::string trim(std::string value)
{
    auto not_space = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
    return value;
}

inline std::string random_bytes_hex(std::size_t count)
{
    std::vector<unsigned char> bytes(count);
    if (RAND_bytes(bytes.data(), static_cast<int>(bytes.size())) != 1) {
        throw ServiceError(500, "INTERNAL", "secure random generation failed", true);
    }
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (unsigned char byte : bytes) out << std::setw(2) << static_cast<int>(byte);
    return out.str();
}

inline std::string uuid4()
{
    auto value = random_bytes_hex(16);
    value[12] = '4';
    const char variant[] = "89ab";
    value[16] = variant[std::stoi(value.substr(16, 1), nullptr, 16) & 0x3];
    return value.substr(0, 8) + "-" + value.substr(8, 4) + "-" + value.substr(12, 4) + "-" +
           value.substr(16, 4) + "-" + value.substr(20, 12);
}

inline std::string sha256_hex(std::string_view input)
{
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int length = 0;
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) throw ServiceError(500, "INTERNAL", "hash context allocation failed", true);
    const bool ok = EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) == 1 &&
                    EVP_DigestUpdate(ctx, input.data(), input.size()) == 1 &&
                    EVP_DigestFinal_ex(ctx, digest, &length) == 1;
    EVP_MD_CTX_free(ctx);
    if (!ok) throw ServiceError(500, "INTERNAL", "hash operation failed", true);
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (unsigned int i = 0; i < length; ++i) out << std::setw(2) << static_cast<int>(digest[i]);
    return out.str();
}

inline std::vector<std::string> split(std::string_view value, char delimiter)
{
    std::vector<std::string> parts;
    std::size_t start = 0;
    while (start <= value.size()) {
        auto end = value.find(delimiter, start);
        if (end == std::string_view::npos) {
            parts.emplace_back(value.substr(start));
            break;
        }
        parts.emplace_back(value.substr(start, end - start));
        start = end + 1;
    }
    return parts;
}

inline bool verify_password(const std::string& password, const std::string& encoded)
{
    auto parts = split(encoded, '$');
    if (parts.size() != 4 || parts[0] != "pbkdf2_sha256") return false;
    const int iterations = std::stoi(parts[1]);
    const auto salt = split(parts[2], ':');
    (void)salt;
    std::vector<unsigned char> salt_bytes;
    const std::string& salt_hex = parts[2];
    if (salt_hex.size() % 2 != 0) return false;
    for (std::size_t i = 0; i < salt_hex.size(); i += 2) {
        salt_bytes.push_back(static_cast<unsigned char>(std::stoi(salt_hex.substr(i, 2), nullptr, 16)));
    }
    unsigned char output[32];
    if (PKCS5_PBKDF2_HMAC(password.data(), static_cast<int>(password.size()), salt_bytes.data(),
                          static_cast<int>(salt_bytes.size()), iterations, EVP_sha256(), sizeof(output), output) != 1) {
        return false;
    }
    std::ostringstream hex;
    hex << std::hex << std::setfill('0');
    for (unsigned char byte : output) hex << std::setw(2) << static_cast<int>(byte);
    const std::string actual = hex.str();
    return actual.size() == parts[3].size() && CRYPTO_memcmp(actual.data(), parts[3].data(), actual.size()) == 0;
}

inline std::string request_id_from(std::string value)
{
    value = trim(std::move(value));
    if (value.empty()) return "req-" + uuid4();
    if (value.size() > 100) return value.substr(0, 100);
    return value;
}

} // namespace ws