#include "core.hpp"
#include <set>

namespace ws {
namespace {

Json message_json(PgResult& r, int row)
{
    return {
        {"id", r.get(row, 0)}, {"conversation_id", r.get(row, 1)}, {"seq", std::stoll(r.get(row, 2))},
        {"sender_type", r.get(row, 3)}, {"sender_id", r.is_null(row, 4) ? Json(nullptr) : Json(r.get(row, 4))},
        {"role", r.get(row, 5)}, {"content", r.get(row, 6)},
        {"run_id", r.is_null(row, 7) ? Json(nullptr) : Json(r.get(row, 7))},
        {"status", r.get(row, 8)}, {"created_at", r.get(row, 9)}};
}

const char* kMessageColumns =
    "m.id::text, m.conversation_id::text, m.seq::text, m.sender_type, m.sender_id::text, "
    "m.role, m.content, m.run_id::text, m.status, to_char(m.created_at AT TIME ZONE 'UTC', "
    "'YYYY-MM-DD\"T\"HH24:MI:SS.MS\"Z\"')";

Json run_json(PgResult& r, int row)
{
    return {
        {"id", r.get(row, 0)}, {"conversation_id", r.get(row, 1)}, {"status", r.get(row, 2)},
        {"cancel_requested", r.get(row, 3) == "t"},
        {"error_code", r.is_null(row, 4) ? Json(nullptr) : Json(r.get(row, 4))},
        {"created_at", r.get(row, 5)},
        {"started_at", r.is_null(row, 6) ? Json(nullptr) : Json(r.get(row, 6))},
        {"finished_at", r.is_null(row, 7) ? Json(nullptr) : Json(r.get(row, 7))}};
}

const char* kRunColumns =
    "r.id::text, r.conversation_id::text, r.status, r.cancel_requested, r.error_code, "
    "to_char(r.created_at AT TIME ZONE 'UTC', 'YYYY-MM-DD\"T\"HH24:MI:SS.MS\"Z\"'), "
    "CASE WHEN r.started_at IS NULL THEN NULL ELSE to_char(r.started_at AT TIME ZONE 'UTC', 'YYYY-MM-DD\"T\"HH24:MI:SS.MS\"Z\"') END, "
    "CASE WHEN r.finished_at IS NULL THEN NULL ELSE to_char(r.finished_at AT TIME ZONE 'UTC', 'YYYY-MM-DD\"T\"HH24:MI:SS.MS\"Z\"') END";

} // namespace

CoreService::CoreService(Config config) : config_(std::move(config)), db_(config_.database_url) {}

void CoreService::require_uuid(const std::string& value, const std::string& field)
{
    if (value.size() != 36 || value[8] != '-' || value[13] != '-' || value[18] != '-' || value[23] != '-')
        throw ServiceError(400, "VALIDATION_FAILED", field + " must be a UUID");
}

void CoreService::require_text(const std::string& value, const std::string& field, std::size_t max_length)
{
    if (trim(value).empty()) throw ServiceError(400, "VALIDATION_FAILED", field + " is required");
    if (value.size() > max_length) throw ServiceError(400, "VALIDATION_FAILED", field + " is too long");
}

bool CoreService::retryable_error(const std::string& code)
{
    return code == "MODEL_TIMEOUT" || code == "MODEL_PROVIDER_ERROR" || code == "INTERNAL";
}

void CoreService::audit(PgSession& s, const std::string& actor_type, const std::string& actor_id,
                        const std::string& action, const std::string& resource_type, const std::string& resource_id,
                        const std::string& result, const std::string& request_id, const Json& metadata)
{
    s.query(
        "INSERT INTO audit_events(actor_type,actor_id,action,resource_type,resource_id,result,request_id,metadata_jsonb) "
        "VALUES($1,$2,$3,$4,$5,$6,$7,$8::jsonb)",
        {actor_type, actor_id.empty() ? std::nullopt : std::optional<std::string>(actor_id), action,
         resource_type, resource_id.empty() ? std::nullopt : std::optional<std::string>(resource_id),
         result, request_id, metadata.dump()});
}

Json CoreService::login(const std::string& username, const std::string& password, const std::string& request_id)
{
    require_text(username, "username", 80);
    require_text(password, "password", 200);
    if (password.size() < 8) throw ServiceError(400, "VALIDATION_FAILED", "password must be at least 8 characters");
    auto s = db_.session();
    auto r = s.query("SELECT id::text,password_hash,display_name,status FROM users WHERE lower(username)=lower($1) LIMIT 1", {username});
    if (r.rows() != 1 || r.get(0, 3) != "active" || !verify_password(password, r.get(0, 1))) {
        audit(s, "system", {}, "auth.login", "session", {}, "denied", request_id);
        throw ServiceError(401, "AUTH_INVALID", "invalid username or password");
    }
    const std::string user_id = r.get(0, 0);
    const std::string token = random_bytes_hex(32);
    s.query("INSERT INTO sessions(id,user_id,token_hash,expires_at) VALUES($1,$2,$3,now()+($4||' seconds')::interval)",
            {uuid4(), user_id, sha256_hex(token), std::to_string(config_.session_ttl_seconds)});
    audit(s, "user", user_id, "auth.login", "session", {}, "success", request_id);
    return {{"user", {{"id", user_id}, {"username", username}, {"display_name", r.get(0, 2)}}}, {"token", token}};
}

void CoreService::logout(const std::string& token, const std::string& user_id, const std::string& request_id)
{
    if (token.empty()) throw ServiceError(401, "AUTH_REQUIRED", "authentication required");
    auto s = db_.session();
    s.query("UPDATE sessions SET revoked_at=now() WHERE token_hash=$1 AND revoked_at IS NULL", {sha256_hex(token)});
    audit(s, "user", user_id, "auth.logout", "session", {}, "success", request_id);
}

AuthSession CoreService::authenticate(const std::string& token)
{
    if (token.empty()) throw ServiceError(401, "AUTH_REQUIRED", "authentication required");
    auto s = db_.session();
    auto r = s.query(
        "SELECT u.id::text,u.username,u.display_name,to_char(s.expires_at AT TIME ZONE 'UTC','YYYY-MM-DD\"T\"HH24:MI:SS.MS\"Z\"') "
        "FROM sessions s JOIN users u ON u.id=s.user_id WHERE s.token_hash=$1 AND s.revoked_at IS NULL "
        "AND s.expires_at>now() AND u.status='active' LIMIT 1", {sha256_hex(token)});
    if (r.rows() != 1) throw ServiceError(401, "AUTH_REQUIRED", "session expired or invalid");
    s.query("UPDATE sessions SET last_seen_at=now() WHERE token_hash=$1", {sha256_hex(token)});
    return {r.get(0, 0), r.get(0, 1), r.get(0, 2), token, r.get(0, 3)};
}

Json CoreService::currentUser(const std::string& user_id)
{
    auto s = db_.session();
    auto r = s.query("SELECT id::text,username,display_name FROM users WHERE id=$1 AND status='active'", {user_id});
    if (r.rows() != 1) throw ServiceError(401, "AUTH_REQUIRED", "user unavailable");
    return {{"id", r.get(0, 0)}, {"username", r.get(0, 1)}, {"display_name", r.get(0, 2)}};
}
Json CoreService::listAgents(const std::string& user_id, const std::string&)
{
    auto s = db_.session();
    auto r = s.query(
        "SELECT a.id::text,a.name,a.avatar,a.system_prompt,a.status,"
        "to_char(a.created_at AT TIME ZONE 'UTC','YYYY-MM-DD\"T\"HH24:MI:SS.MS\"Z\"'),"
        "p.id::text,p.name,p.provider_type,p.model_name FROM agents a "
        "JOIN model_providers p ON p.id=a.model_provider_id WHERE a.owner_id=$1 ORDER BY a.created_at DESC", {user_id});
    Json items = Json::array();
    for (int row = 0; row < r.rows(); ++row) {
        items.push_back({
            {"id", r.get(row, 0)}, {"name", r.get(row, 1)},
            {"avatar", r.is_null(row, 2) ? Json(nullptr) : Json(r.get(row, 2))},
            {"system_prompt", r.get(row, 3)}, {"status", r.get(row, 4)}, {"created_at", r.get(row, 5)},
            {"model_provider", {{"id", r.get(row, 6)}, {"name", r.get(row, 7)},
                                {"provider_type", r.get(row, 8)}, {"model_name", r.get(row, 9)}}}});
    }
    return {{"items", items}};
}

Json CoreService::createAgent(const std::string& user_id, const Json& body, const std::string& request_id)
{
    const std::string name = body.value("name", "");
    const std::string system_prompt = body.value("system_prompt", "");
    require_text(name, "name", 80);
    if (system_prompt.size() > 8000) throw ServiceError(400, "VALIDATION_FAILED", "system_prompt is too long");
    if (body.contains("avatar") && !body["avatar"].is_null() && !body["avatar"].is_string())
        throw ServiceError(400, "VALIDATION_FAILED", "avatar must be a string or null");
    const std::string avatar = body.contains("avatar") && body["avatar"].is_string() ? body["avatar"].get<std::string>() : "";
    auto s = db_.session();
    PgResult provider;
    if (body.contains("model_provider_id") && !body["model_provider_id"].is_null()) {
        const std::string provider_id = body["model_provider_id"].get<std::string>();
        require_uuid(provider_id, "model_provider_id");
        provider = s.query("SELECT id::text FROM model_providers WHERE id=$1 AND enabled=true", {provider_id});
    } else {
        provider = s.query("SELECT id::text FROM model_providers WHERE enabled=true ORDER BY created_at LIMIT 1");
    }
    if (provider.rows() != 1) throw ServiceError(400, "VALIDATION_FAILED", "model provider is unavailable");
    const std::string agent_id = uuid4();
    s.begin();
    try {
        s.query("INSERT INTO agents(id,owner_id,name,avatar,system_prompt,model_provider_id) VALUES($1,$2,$3,$4,$5,$6)",
                {agent_id, user_id, name, avatar.empty() ? std::nullopt : std::optional<std::string>(avatar),
                 system_prompt, provider.get(0, 0)});
        audit(s, "user", user_id, "agent.create", "agent", agent_id, "success", request_id);
        s.commit();
    } catch (...) {
        s.rollback();
        throw;
    }
    auto items = listAgents(user_id, request_id)["items"];
    for (const auto& item : items) if (item["id"] == agent_id) return item;
    throw ServiceError(500, "INTERNAL", "agent created but unavailable", true);
}

Json CoreService::listConversations(const std::string& user_id, const std::string&)
{
    auto s = db_.session();
    auto r = s.query(
        "SELECT c.id::text,c.agent_id::text,a.name,c.title,c.status,"
        "to_char(c.created_at AT TIME ZONE 'UTC','YYYY-MM-DD\"T\"HH24:MI:SS.MS\"Z\"'),"
        "to_char(c.updated_at AT TIME ZONE 'UTC','YYYY-MM-DD\"T\"HH24:MI:SS.MS\"Z\"') "
        "FROM conversations c JOIN agents a ON a.id=c.agent_id WHERE c.owner_id=$1 ORDER BY c.updated_at DESC", {user_id});
    Json items = Json::array();
    for (int row = 0; row < r.rows(); ++row) {
        items.push_back({{"id", r.get(row, 0)}, {"agent_id", r.get(row, 1)}, {"agent_name", r.get(row, 2)},
                         {"title", r.get(row, 3)}, {"status", r.get(row, 4)}, {"created_at", r.get(row, 5)},
                         {"updated_at", r.get(row, 6)}});
    }
    return {{"items", items}};
}

Json CoreService::createConversation(const std::string& user_id, const Json& body, const std::string& request_id)
{
    const std::string agent_id = body.value("agent_id", "");
    require_uuid(agent_id, "agent_id");
    std::string title = body.value("title", "");
    if (trim(title).empty()) title = "New conversation";
    require_text(title, "title", 200);
    auto s = db_.session();
    auto agent = s.query("SELECT id::text,name FROM agents WHERE id=$1 AND owner_id=$2 AND status='active'", {agent_id, user_id});
    if (agent.rows() != 1) throw ServiceError(403, "FORBIDDEN", "agent is not accessible");
    const std::string id = uuid4();
    s.begin();
    try {
        auto r = s.query(
            "INSERT INTO conversations(id,owner_id,agent_id,title) VALUES($1,$2,$3,$4) RETURNING id::text,"
            "to_char(created_at AT TIME ZONE 'UTC','YYYY-MM-DD\"T\"HH24:MI:SS.MS\"Z\"'),"
            "to_char(updated_at AT TIME ZONE 'UTC','YYYY-MM-DD\"T\"HH24:MI:SS.MS\"Z\"')",
            {id, user_id, agent_id, title});
        audit(s, "user", user_id, "conversation.create", "conversation", id, "success", request_id);
        s.commit();
        return {{"id", r.get(0, 0)}, {"agent_id", agent_id}, {"agent_name", agent.get(0, 1)},
                {"title", title}, {"status", "active"}, {"created_at", r.get(0, 1)}, {"updated_at", r.get(0, 2)}};
    } catch (...) {
        s.rollback();
        throw;
    }
}

Json CoreService::listMessages(const std::string& user_id, const std::string& conversation_id,
                               long long after_seq, const std::string&)
{
    require_uuid(conversation_id, "conversation_id");
    auto s = db_.session();
    auto owner = s.query("SELECT 1 FROM conversations WHERE id=$1 AND owner_id=$2", {conversation_id, user_id});
    if (owner.rows() != 1) throw ServiceError(403, "FORBIDDEN", "conversation is not accessible");
    auto r = s.query(std::string("SELECT ") + kMessageColumns +
                         " FROM messages m WHERE m.conversation_id=$1 AND m.seq>$2 ORDER BY m.seq",
                     {conversation_id, std::to_string(after_seq)});
    Json items = Json::array();
    for (int row = 0; row < r.rows(); ++row) items.push_back(message_json(r, row));
    return {{"items", items}};
}

Json CoreService::sendMessage(const std::string& user_id, const std::string& conversation_id, const Json& body,
                              const std::string& client_request_id, const std::string& request_id)
{
    require_uuid(conversation_id, "conversation_id");
    const std::string content = body.value("content", "");
    require_text(content, "content", 32000);
    if (client_request_id.size() > 100) throw ServiceError(400, "VALIDATION_FAILED", "client_request_id is too long");
    auto s = db_.session();
    if (!client_request_id.empty()) {
        auto existing = s.query(
            std::string("SELECT ") + kMessageColumns + ",r.id::text,r.status FROM messages m "
            "JOIN conversations c ON c.id=m.conversation_id JOIN runs r ON r.trigger_message_id=m.id "
            "WHERE c.owner_id=$1 AND m.conversation_id=$2 AND m.client_request_id=$3 AND m.sender_type='user' LIMIT 1",
            {user_id, conversation_id, client_request_id});
        if (existing.rows() == 1) {
            return {{"message", message_json(existing, 0)}, {"run_id", existing.get(0, 10)},
                    {"status", existing.get(0, 11)}, {"events_url", "/api/v1/runs/" + existing.get(0, 10) + "/events"},
                    {"duplicate", true}};
        }
    }
    auto conversation = s.query(
        "SELECT c.agent_id::text,a.model_provider_id::text FROM conversations c JOIN agents a ON a.id=c.agent_id "
        "WHERE c.id=$1 AND c.owner_id=$2 AND c.status='active' AND a.status='active'", {conversation_id, user_id});
    if (conversation.rows() != 1) throw ServiceError(403, "FORBIDDEN", "conversation is not accessible");
    const std::string message_id = uuid4();
    const std::string run_id = uuid4();
    const std::string job_id = uuid4();
    s.begin();
    try {
        auto msg = s.query(
            "INSERT INTO messages(id,conversation_id,sender_type,sender_id,role,content,status,client_request_id) "
            "VALUES($1,$2,'user',$3,'user',$4,'completed',$5) RETURNING id::text,conversation_id::text,seq::text,"
            "sender_type,sender_id::text,role,content,run_id::text,status,"
            "to_char(created_at AT TIME ZONE 'UTC','YYYY-MM-DD\"T\"HH24:MI:SS.MS\"Z\"')",
            {message_id, conversation_id, user_id, content,
             client_request_id.empty() ? std::nullopt : std::optional<std::string>(client_request_id)});
        s.query("INSERT INTO runs(id,conversation_id,agent_id,trigger_message_id,model_provider_id,status) "
                "VALUES($1,$2,$3,$4,$5,'queued')",
                {run_id, conversation_id, conversation.get(0, 0), message_id, conversation.get(0, 1)});
        s.query("UPDATE messages SET run_id=$1 WHERE id=$2", {run_id, message_id});
        s.query("INSERT INTO jobs(id,run_id,status) VALUES($1,$2,'queued')", {job_id, run_id});
        s.query("UPDATE conversations SET updated_at=now() WHERE id=$1", {conversation_id});
        audit(s, "user", user_id, "message.send", "run", run_id, "success", request_id,
              {{"conversation_id", conversation_id}});
        s.commit();
        Json message = message_json(msg, 0);
        message["run_id"] = run_id;
        return {{"message", message}, {"run_id", run_id}, {"status", "queued"},
                {"events_url", "/api/v1/runs/" + run_id + "/events"}, {"duplicate", false}};
    } catch (...) {
        s.rollback();
        throw;
    }
}
Json CoreService::retryRun(const std::string& user_id, const std::string& run_id, const std::string& request_id)
{
    require_uuid(run_id, "run_id");
    auto s = db_.session();
    auto old = s.query(
        "SELECT r.conversation_id::text,r.agent_id::text,r.trigger_message_id::text,r.model_provider_id::text,r.status "
        "FROM runs r JOIN conversations c ON c.id=r.conversation_id WHERE r.id=$1 AND c.owner_id=$2", {run_id, user_id});
    if (old.rows() != 1) throw ServiceError(403, "FORBIDDEN", "run is not accessible");
    if (old.get(0, 4) != "failed" && old.get(0, 4) != "cancelled")
        throw ServiceError(409, "CONFLICT", "only failed or cancelled runs can be retried");
    const std::string new_run_id = uuid4();
    const std::string job_id = uuid4();
    s.begin();
    try {
        s.query("INSERT INTO runs(id,conversation_id,agent_id,trigger_message_id,model_provider_id,status) "
                "VALUES($1,$2,$3,$4,$5,'queued')",
                {new_run_id, old.get(0, 0), old.get(0, 1), old.get(0, 2), old.get(0, 3)});
        s.query("INSERT INTO jobs(id,run_id,status) VALUES($1,$2,'queued')", {job_id, new_run_id});
        s.query("UPDATE conversations SET updated_at=now() WHERE id=$1", {old.get(0, 0)});
        audit(s, "user", user_id, "run.retry", "run", new_run_id, "success", request_id, {{"source_run_id", run_id}});
        auto msg = s.query(std::string("SELECT ") + kMessageColumns + " FROM messages m WHERE m.id=$1", {old.get(0, 2)});
        s.commit();
        Json message = message_json(msg, 0);
        message["run_id"] = new_run_id;
        return {{"message", message}, {"run_id", new_run_id}, {"status", "queued"},
                {"events_url", "/api/v1/runs/" + new_run_id + "/events"}, {"duplicate", false}};
    } catch (...) {
        s.rollback();
        throw;
    }
}

Json CoreService::cancelRun(const std::string& user_id, const std::string& run_id, const std::string& request_id)
{
    require_uuid(run_id, "run_id");
    auto s = db_.session();
    s.begin();
    try {
        auto r = s.query("SELECT " + std::string(kRunColumns) + ",j.status FROM runs r JOIN conversations c ON c.id=r.conversation_id "
                         "JOIN jobs j ON j.run_id=r.id WHERE r.id=$1 AND c.owner_id=$2 FOR UPDATE OF r,j",
                         {run_id, user_id});
        if (r.rows() != 1) throw ServiceError(403, "FORBIDDEN", "run is not accessible");
        const std::string status = r.get(0, 2);
        if (status == "succeeded" || status == "failed" || status == "cancelled")
            throw ServiceError(409, "RUN_NOT_CANCELLABLE", "run is already terminal");
        if (r.get(0, 8) == "queued") {
            const std::string next_seq = std::to_string(std::stoll(s.query(
                "SELECT COALESCE(MAX(seq),0)+1 FROM run_events WHERE run_id=$1", {run_id}).get(0, 0)) );
            s.query("UPDATE jobs SET status='cancelled',lease_token=NULL,lease_until=NULL,updated_at=now() WHERE run_id=$1", {run_id});
            s.query("UPDATE runs SET status='cancelled',cancel_requested=true,finished_at=now() WHERE id=$1", {run_id});
            s.query("INSERT INTO run_events(run_id,seq,event_type,payload_jsonb,request_id) "
                    "VALUES($1,$2,'run.cancelled',$3::jsonb,$4) ON CONFLICT (run_id,event_type) "
                    "WHERE event_type IN ('run.started','message.completed','run.failed','run.cancelled') DO NOTHING",
                    {run_id, next_seq, Json{{"reason", "user_requested"}}.dump(), request_id});
        } else {
            s.query("UPDATE runs SET cancel_requested=true WHERE id=$1", {run_id});
        }
        audit(s, "user", user_id, "run.cancel", "run", run_id, "success", request_id);
        s.commit();
    } catch (...) {
        s.rollback();
        throw;
    }
    return getRun(user_id, run_id);
}

Json CoreService::getRun(const std::string& user_id, const std::string& run_id)
{
    require_uuid(run_id, "run_id");
    auto s = db_.session();
    auto r = s.query(std::string("SELECT ") + kRunColumns + " FROM runs r JOIN conversations c ON c.id=r.conversation_id "
                     "WHERE r.id=$1 AND c.owner_id=$2", {run_id, user_id});
    if (r.rows() != 1) throw ServiceError(403, "FORBIDDEN", "run is not accessible");
    return run_json(r, 0);
}

Json CoreService::listRunEvents(const std::string& user_id, const std::string& run_id, long long after_seq)
{
    require_uuid(run_id, "run_id");
    auto s = db_.session();
    auto access = s.query("SELECT 1 FROM runs r JOIN conversations c ON c.id=r.conversation_id "
                          "WHERE r.id=$1 AND c.owner_id=$2", {run_id, user_id});
    if (access.rows() != 1) throw ServiceError(403, "FORBIDDEN", "run is not accessible");
    auto r = s.query(
        "SELECT seq::text,event_type,payload_jsonb::text,request_id,"
        "to_char(created_at AT TIME ZONE 'UTC','YYYY-MM-DD\"T\"HH24:MI:SS.MS\"Z\"') "
        "FROM run_events WHERE run_id=$1 AND seq>$2 ORDER BY run_events.seq", {run_id, std::to_string(after_seq)});
    Json items = Json::array();
    for (int row = 0; row < r.rows(); ++row) {
        Json payload = Json::parse(r.get(row, 2));
        items.push_back({{"schema_version", "1.0"}, {"request_id", r.get(row, 3)}, {"run_id", run_id},
                         {"seq", std::stoll(r.get(row, 0))}, {"type", r.get(row, 1)},
                         {"occurred_at", r.get(row, 4)}, {"payload", payload}});
    }
    return {{"items", items}};
}

Json CoreService::health()
{
    const bool ok = db_.healthy();
    return {{"status", ok ? "ok" : "degraded"}, {"database", ok ? "ok" : "unavailable"}, {"version", "1.0.0"}};
}
Json CoreService::claimNextJob(const std::string& worker_id, const std::string& request_id)
{
    if (trim(worker_id).empty()) throw ServiceError(400, "VALIDATION_FAILED", "worker_id is required");
    for (int loop = 0; loop < 5; ++loop) {
        auto s = db_.session();
        s.begin();
        try {
            auto r = s.query(
                "SELECT j.id::text,j.run_id::text,j.attempt_count,j.max_attempts,j.status,"
                "COALESCE(j.lease_until<now(),false) AS expired FROM jobs j "
                "JOIN runs r ON r.id=j.run_id WHERE j.status='queued' OR "
                "(j.status='running' AND j.lease_until<now()) ORDER BY j.created_at "
                "FOR UPDATE OF j SKIP LOCKED LIMIT 1");
            if (r.rows() == 0) {
                s.commit();
                return {{"has_job", false}, {"poll_after_ms", 750}};
            }
            const std::string job_id = r.get(0, 0);
            const std::string run_id = r.get(0, 1);
            const int attempt = std::stoi(r.get(0, 2));
            const int max_attempts = std::stoi(r.get(0, 3));
            const std::string status = r.get(0, 4);
            if (status == "running") {
                if (attempt >= max_attempts) {
                    auto next = s.query("SELECT COALESCE(MAX(seq),0)+1 FROM run_events WHERE run_id=$1", {run_id});
                    s.query("UPDATE jobs SET status='failed',last_error_code='MODEL_TIMEOUT',lease_token=NULL,lease_until=NULL,updated_at=now() WHERE id=$1", {job_id});
                    s.query("UPDATE runs SET status='failed',error_code='MODEL_TIMEOUT',finished_at=now() WHERE id=$1", {run_id});
                    s.query("INSERT INTO run_events(run_id,seq,event_type,payload_jsonb,request_id) VALUES($1,$2,'run.failed',$3::jsonb,$4) "
                            "ON CONFLICT (run_id,event_type) WHERE event_type IN ('run.started','message.completed','run.failed','run.cancelled') DO NOTHING",
                            {run_id, next.get(0, 0), Json{{"error_code", "MODEL_TIMEOUT"}, {"message", "worker lease expired"}, {"retryable", false}}.dump(), request_id});
                } else {
                    s.query("UPDATE jobs SET status='queued',lease_token=NULL,lease_until=NULL,updated_at=now() WHERE id=$1", {job_id});
                    s.query("UPDATE runs SET status='queued' WHERE id=$1", {run_id});
                }
                s.commit();
                continue;
            }
            const std::string lease = random_bytes_hex(32);
            auto leased = s.query(
                "UPDATE jobs SET status='running',lease_token=$2,lease_until=now()+'30 seconds'::interval,"
                "attempt_count=attempt_count+1,updated_at=now() WHERE id=$1 "
                "RETURNING attempt_count,(extract(epoch FROM lease_until)*1000)::bigint::text", {job_id, lease});
            s.query("UPDATE runs SET status='running',started_at=COALESCE(started_at,now()) WHERE id=$1", {run_id});
            auto next = s.query("SELECT COALESCE(MAX(seq),0)+1 FROM run_events WHERE run_id=$1", {run_id});
            s.query("INSERT INTO run_events(run_id,seq,event_type,payload_jsonb,request_id) VALUES($1,$2,'run.started',$3::jsonb,$4) "
                    "ON CONFLICT (run_id,event_type) WHERE event_type IN ('run.started','message.completed','run.failed','run.cancelled') DO NOTHING",
                    {run_id, next.get(0, 0), Json{{"job_id", job_id}, {"attempt", std::stoi(leased.get(0, 0))}}.dump(), request_id});
            audit(s, "worker", worker_id, "job.claim", "job", job_id, "success", request_id, {{"run_id", run_id}});
            s.commit();
            return {{"has_job", true}, {"job", {{"job_id", job_id}, {"run_id", run_id}, {"worker_id", worker_id},
                    {"lease_token", lease}, {"lease_expires_at_unix_ms", std::stoll(leased.get(0, 1))},
                    {"attempt", std::stoi(leased.get(0, 0))}, {"max_attempts", max_attempts}, {"status", "running"}}},
                    {"poll_after_ms", 0}};
        } catch (...) {
            s.rollback();
            throw;
        }
    }
    return {{"has_job", false}, {"poll_after_ms", 1000}};
}

Json CoreService::getExecutionContext(const std::string& job_id, const std::string& run_id,
                                      const std::string& lease_token, const std::string&)
{
    require_uuid(job_id, "job_id");
    require_uuid(run_id, "run_id");
    auto s = db_.session();
    auto r = s.query(
        "SELECT r.agent_id::text,r.conversation_id::text,a.system_prompt,p.provider_type,p.model_name,"
        "COALESCE(p.base_url,''),COALESCE(p.secret_ref,''),r.trigger_message_id::text,"
        "(extract(epoch FROM j.lease_until)*1000)::bigint::text,j.attempt_count,j.max_attempts "
        "FROM jobs j JOIN runs r ON r.id=j.run_id JOIN agents a ON a.id=r.agent_id "
        "JOIN model_providers p ON p.id=r.model_provider_id WHERE j.id=$1 AND r.id=$2 "
        "AND j.lease_token=$3 AND j.status='running' AND j.lease_until>now()",
        {job_id, run_id, lease_token});
    if (r.rows() != 1) throw ServiceError(409, "JOB_LEASE_INVALID", "job lease is invalid or expired", true);
    auto history = s.query(
        "SELECT m.id::text,m.role,m.sender_type,m.content,m.seq::text FROM messages m "
        "JOIN messages trigger ON trigger.id=$2 WHERE m.conversation_id=$1 AND m.seq<=trigger.seq "
        "AND m.status='completed' AND (m.run_id IS NULL OR m.run_id<>$3 OR m.id=$2) ORDER BY m.seq",
        {r.get(0, 1), r.get(0, 7), run_id});
    auto next = s.query("SELECT COALESCE(MAX(seq),0)+1 FROM run_events WHERE run_id=$1", {run_id});
    Json messages = Json::array();
    for (int row = 0; row < history.rows(); ++row) {
        messages.push_back({{"message_id", history.get(row, 0)}, {"role", history.get(row, 1)},
                            {"sender_type", history.get(row, 2)}, {"content", history.get(row, 3)},
                            {"seq", std::stoll(history.get(row, 4))}});
    }
    return {{"run_id", run_id}, {"job_id", job_id}, {"agent_id", r.get(0, 0)},
            {"conversation_id", r.get(0, 1)}, {"system_prompt", r.get(0, 2)},
            {"provider_type", r.get(0, 3)}, {"model_name", r.get(0, 4)},
            {"base_url", r.get(0, 5)}, {"secret_ref", r.get(0, 6)}, {"messages", messages},
            {"next_event_seq", std::stoll(next.get(0, 0))}, {"lease_expires_at_unix_ms", std::stoll(r.get(0, 8))}};
}
Json CoreService::heartbeat(const std::string& job_id, const std::string& run_id,
                            const std::string& lease_token, int lease_seconds, const std::string&)
{
    require_uuid(job_id, "job_id");
    require_uuid(run_id, "run_id");
    const int seconds = std::clamp(lease_seconds <= 0 ? 20 : lease_seconds, 10, 60);
    auto s = db_.session();
    auto r = s.query(
        "SELECT r.cancel_requested,r.status,j.status FROM jobs j JOIN runs r ON r.id=j.run_id "
        "WHERE j.id=$1 AND r.id=$2 AND j.lease_token=$3 AND j.status='running' AND j.lease_until>now()",
        {job_id, run_id, lease_token});
    if (r.rows() != 1) return {{"lease_valid", false}, {"cancel_requested", false}, {"job_status", "failed"},
                               {"lease_expires_at_unix_ms", 0}};
    auto updated = s.query("UPDATE jobs SET lease_until=now()+($2||' seconds')::interval,updated_at=now() "
                           "WHERE id=$1 RETURNING (extract(epoch FROM lease_until)*1000)::bigint::text",
                           {job_id, std::to_string(seconds)});
    return {{"lease_valid", true}, {"cancel_requested", r.get(0, 0) == "t"}, {"job_status", r.get(0, 2)},
            {"lease_expires_at_unix_ms", std::stoll(updated.get(0, 0))}};
}

Json CoreService::appendEvent(const std::string& job_id, const std::string& run_id,
                              const std::string& lease_token, long long event_seq,
                              const std::string& event_type, const std::string& payload_json,
                              const std::string& request_id)
{
    require_uuid(job_id, "job_id");
    require_uuid(run_id, "run_id");
    if (event_seq <= 0) throw ServiceError(400, "VALIDATION_FAILED", "event_seq must be positive");
    if (event_type != "message.delta") throw ServiceError(400, "VALIDATION_FAILED", "unsupported worker event type");
    if (payload_json.size() > 65536) throw ServiceError(400, "VALIDATION_FAILED", "event payload is too large");
    Json payload;
    try { payload = Json::parse(payload_json); } catch (...) { throw ServiceError(400, "VALIDATION_FAILED", "payload_json is invalid"); }
    if (!payload.is_object()) throw ServiceError(400, "VALIDATION_FAILED", "payload_json must be an object");
    auto s = db_.session();
    s.begin();
    try {
        auto lease = s.query(
            "SELECT r.cancel_requested FROM jobs j JOIN runs r ON r.id=j.run_id WHERE j.id=$1 AND r.id=$2 "
            "AND j.lease_token=$3 AND j.status='running' AND j.lease_until>now() FOR UPDATE OF j,r",
            {job_id, run_id, lease_token});
        if (lease.rows() != 1)
            throw ServiceError(409, "JOB_LEASE_INVALID", "job lease is invalid or expired", true);
        if (lease.get(0, 0) == "t")
            throw ServiceError(409, "RUN_CANCELLED", "run cancellation requested");
        auto inserted = s.query(
            "INSERT INTO run_events(run_id,seq,event_type,payload_jsonb,request_id) VALUES($1,$2,$3,$4::jsonb,$5) "
            "ON CONFLICT (run_id,seq) DO NOTHING RETURNING seq::text",
            {run_id, std::to_string(event_seq), event_type, payload.dump(), request_id});
        bool duplicate = inserted.rows() == 0;
        if (duplicate) {
            auto old = s.query("SELECT event_type,payload_jsonb::text FROM run_events WHERE run_id=$1 AND seq=$2",
                               {run_id, std::to_string(event_seq)});
            if (old.rows() != 1 || old.get(0, 0) != event_type || old.get(0, 1) != payload.dump())
                throw ServiceError(409, "CONFLICT", "event sequence already used with different content");
        }
        s.query("UPDATE jobs SET lease_until=now()+'20 seconds'::interval,updated_at=now() WHERE id=$1", {job_id});
        auto next = s.query("SELECT COALESCE(MAX(seq),0)+1 FROM run_events WHERE run_id=$1", {run_id});
        s.commit();
        return {{"accepted", true}, {"duplicate", duplicate}, {"stored_seq", event_seq},
                {"next_seq", std::stoll(next.get(0, 0))}};
    } catch (...) {
        s.rollback();
        throw;
    }
}
const std::unordered_set<std::string> kAllowedErrors = {"MODEL_TIMEOUT", "MODEL_PROVIDER_ERROR", "RUN_CANCELLED", "INTERNAL"};

Json CoreService::updateJobStatus(const std::string& job_id, const std::string& run_id, const std::string& lease_token,
                                  const std::string& status, const std::string& error_code, const std::string&,
                                  const std::string& final_content, const std::string& usage_json,
                                  const std::string& request_id)
{
    require_uuid(job_id, "job_id");
    require_uuid(run_id, "run_id");
    if (status != "succeeded" && status != "failed" && status != "cancelled")
        throw ServiceError(400, "VALIDATION_FAILED", "unsupported terminal job status");
    if (status == "failed" && !kAllowedErrors.contains(error_code))
        throw ServiceError(400, "VALIDATION_FAILED", "unsupported worker error code");
    if (final_content.size() > 200000)
        throw ServiceError(400, "VALIDATION_FAILED", "final content is too large");
    Json usage = Json::object();
    if (!usage_json.empty()) {
        try { usage = Json::parse(usage_json); } catch (...) { throw ServiceError(400, "VALIDATION_FAILED", "usage_json is invalid"); }
        if (!usage.is_object()) throw ServiceError(400, "VALIDATION_FAILED", "usage_json must be an object");
    }

    auto s = db_.session();
    s.begin();
    try {
        auto r = s.query(
            "SELECT j.status,j.attempt_count,j.max_attempts,r.status,r.cancel_requested,r.agent_id::text,j.lease_token "
            "FROM jobs j JOIN runs r ON r.id=j.run_id WHERE j.id=$1 AND r.id=$2 "
            "FOR UPDATE OF j,r", {job_id, run_id});
        if (r.rows() != 1) throw ServiceError(409, "JOB_LEASE_INVALID", "job lease is invalid", true);
        if (r.get(0, 3) == "succeeded" || r.get(0, 3) == "failed" || r.get(0, 3) == "cancelled")
            return {{"accepted", true}, {"duplicate", true}, {"run_status", r.get(0, 3)}, {"final_event_seq", 0}};

        std::string effective = status;
        if (r.get(0, 4) == "t" && effective == "succeeded") effective = "cancelled";
        if (effective == "succeeded" && trim(final_content).empty())
            throw ServiceError(400, "VALIDATION_FAILED", "successful runs require final content");
        if (effective == "failed" && retryable_error(error_code) &&
            std::stoi(r.get(0, 1)) < std::stoi(r.get(0, 2))) {
            s.query("UPDATE jobs SET status='queued',lease_token=NULL,lease_until=NULL,last_error_code=$2,updated_at=now() WHERE id=$1",
                    {job_id, error_code});
            s.query("UPDATE runs SET status='queued',error_code=$2 WHERE id=$1", {run_id, error_code});
            audit(s, "worker", {}, "job.retry", "job", job_id, "failure", request_id,
                  {{"run_id", run_id}, {"error_code", error_code}, {"attempt", std::stoi(r.get(0, 1))}});
            s.commit();
            return {{"accepted", true}, {"duplicate", false}, {"run_status", "queued"}, {"final_event_seq", 0}};
        }

        auto next = s.query("SELECT COALESCE(MAX(seq),0)+1 FROM run_events WHERE run_id=$1", {run_id});
        const long long final_seq = std::stoll(next.get(0, 0));
        if (effective == "succeeded") {
            auto inserted = s.query(
                "INSERT INTO messages(id,conversation_id,sender_type,sender_id,role,content,run_id,status) "
                "SELECT $1,r.conversation_id,'agent',$2,'assistant',$3,$4,'completed' FROM runs r WHERE r.id=$4 "
                "ON CONFLICT (run_id) WHERE sender_type='agent' AND run_id IS NOT NULL DO NOTHING RETURNING id::text",
                {uuid4(), r.get(0, 5), final_content, run_id});
            std::string message_id;
            if (inserted.rows() == 1) message_id = inserted.get(0, 0);
            else message_id = s.query("SELECT id::text FROM messages WHERE run_id=$1 AND sender_type='agent'", {run_id}).get(0, 0);
            s.query("INSERT INTO run_events(run_id,seq,event_type,payload_jsonb,request_id) VALUES($1,$2,'message.completed',$3::jsonb,$4)",
                    {run_id, std::to_string(final_seq), Json{{"message_id", message_id}, {"content", final_content}, {"usage", usage}}.dump(), request_id});
            s.query("UPDATE jobs SET status='succeeded',lease_token=NULL,lease_until=NULL,updated_at=now() WHERE id=$1", {job_id});
            s.query("UPDATE runs SET status='succeeded',finished_at=now(),error_code=NULL WHERE id=$1", {run_id});
        } else if (effective == "failed") {
            s.query("INSERT INTO run_events(run_id,seq,event_type,payload_jsonb,request_id) VALUES($1,$2,'run.failed',$3::jsonb,$4)",
                    {run_id, std::to_string(final_seq), Json{{"error_code", error_code}, {"message", "model execution failed"}, {"retryable", false}}.dump(), request_id});
            s.query("UPDATE jobs SET status='failed',last_error_code=$2,lease_token=NULL,lease_until=NULL,updated_at=now() WHERE id=$1",
                    {job_id, error_code});
            s.query("UPDATE runs SET status='failed',error_code=$2,finished_at=now() WHERE id=$1", {run_id, error_code});
        } else {
            s.query("INSERT INTO run_events(run_id,seq,event_type,payload_jsonb,request_id) VALUES($1,$2,'run.cancelled',$3::jsonb,$4)",
                    {run_id, std::to_string(final_seq), Json{{"reason", "worker_cancelled"}}.dump(), request_id});
            s.query("UPDATE jobs SET status='cancelled',lease_token=NULL,lease_until=NULL,updated_at=now() WHERE id=$1", {job_id});
            s.query("UPDATE runs SET status='cancelled',error_code='RUN_CANCELLED',cancel_requested=true,finished_at=now() WHERE id=$1", {run_id});
        }
        audit(s, "worker", {}, "job." + effective, "job", job_id, effective == "succeeded" ? "success" : "failure",
              request_id, {{"run_id", run_id}, {"error_code", effective == "succeeded" ? "" : error_code}});
        s.commit();
        return {{"accepted", true}, {"duplicate", false}, {"run_status", effective}, {"final_event_seq", final_seq}};
    } catch (...) {
        s.rollback();
        throw;
    }
}
std::pair<bool, std::string> CoreService::checkPermission(const std::string& actor_user_id,
                                                          const std::string& resource_type,
                                                          const std::string& resource_id,
                                                          const std::string& action)
{
    require_uuid(actor_user_id, "actor_user_id");
    require_uuid(resource_id, "resource_id");
    if (action.empty()) return {false, "action is required"};
    auto s = db_.session();
    PgResult r;
    if (resource_type == "AGENT") {
        r = s.query("SELECT 1 FROM agents WHERE id=$1 AND owner_id=$2", {resource_id, actor_user_id});
    } else if (resource_type == "CONVERSATION") {
        r = s.query("SELECT 1 FROM conversations WHERE id=$1 AND owner_id=$2", {resource_id, actor_user_id});
    } else if (resource_type == "RUN") {
        r = s.query("SELECT 1 FROM runs r JOIN conversations c ON c.id=r.conversation_id "
                    "WHERE r.id=$1 AND c.owner_id=$2", {resource_id, actor_user_id});
    } else {
        return {false, "unsupported resource type"};
    }
    return r.rows() == 1 ? std::pair{true, std::string("owner")} : std::pair{false, std::string("not owner")};
}

} // namespace ws