#pragma once

#include "database.hpp"
#include <string>
#include <unordered_set>
#include <utility>

namespace ws {

struct AuthSession {
    std::string user_id;
    std::string username;
    std::string display_name;
    std::string token;
    std::string expires_at;
};

class CoreService {
public:
    explicit CoreService(Config config);
    CoreService(const CoreService&) = delete;
    CoreService& operator=(const CoreService&) = delete;

    Json login(const std::string& username, const std::string& password, const std::string& request_id);
    void logout(const std::string& token, const std::string& user_id, const std::string& request_id);
    AuthSession authenticate(const std::string& token);
    Json currentUser(const std::string& user_id);

    Json listAgents(const std::string& user_id, const std::string& request_id);
    Json createAgent(const std::string& user_id, const Json& body, const std::string& request_id);
    Json listConversations(const std::string& user_id, const std::string& request_id);
    Json createConversation(const std::string& user_id, const Json& body, const std::string& request_id);
    Json listMessages(const std::string& user_id, const std::string& conversation_id, long long after_seq, const std::string& request_id);
    Json sendMessage(const std::string& user_id, const std::string& conversation_id, const Json& body,
                     const std::string& client_request_id, const std::string& request_id);
    Json retryRun(const std::string& user_id, const std::string& run_id, const std::string& request_id);
    Json cancelRun(const std::string& user_id, const std::string& run_id, const std::string& request_id);
    Json getRun(const std::string& user_id, const std::string& run_id);
    Json listRunEvents(const std::string& user_id, const std::string& run_id, long long after_seq);
    Json health();

    Json claimNextJob(const std::string& worker_id, const std::string& request_id);
    Json getExecutionContext(const std::string& job_id, const std::string& run_id,
                             const std::string& lease_token, const std::string& request_id);
    Json heartbeat(const std::string& job_id, const std::string& run_id,
                   const std::string& lease_token, int lease_seconds, const std::string& request_id);
    Json appendEvent(const std::string& job_id, const std::string& run_id, const std::string& lease_token,
                     long long event_seq, const std::string& event_type, const std::string& payload_json,
                     const std::string& request_id);
    Json updateJobStatus(const std::string& job_id, const std::string& run_id, const std::string& lease_token,
                         const std::string& status, const std::string& error_code, const std::string& error_message,
                         const std::string& final_content, const std::string& usage_json, const std::string& request_id);
    std::pair<bool, std::string> checkPermission(const std::string& actor_user_id, const std::string& resource_type,
                                                  const std::string& resource_id, const std::string& action);

private:
    void audit(PgSession& session, const std::string& actor_type, const std::string& actor_id,
               const std::string& action, const std::string& resource_type, const std::string& resource_id,
               const std::string& result, const std::string& request_id,
               const Json& metadata = Json::object());
    static void require_uuid(const std::string& value, const std::string& field);
    static void require_text(const std::string& value, const std::string& field, std::size_t max_length);
    static bool retryable_error(const std::string& error_code);

    Config config_;
    Database db_;
};

} // namespace ws