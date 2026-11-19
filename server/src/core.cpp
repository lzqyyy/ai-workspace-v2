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

const char* kProjectColumns =
    "p.id::text, p.name, p.description, p.status, p.created_by_user_id::text, "
    "to_char(p.created_at AT TIME ZONE 'UTC', 'YYYY-MM-DD\"T\"HH24:MI:SS.MS\"Z\"'), "
    "to_char(p.updated_at AT TIME ZONE 'UTC', 'YYYY-MM-DD\"T\"HH24:MI:SS.MS\"Z\"')";

const char* kProjectMemberColumns =
    "pm.project_id::text, pm.user_id::text, u.username, u.display_name, pm.role, pm.added_by_user_id::text, "
    "to_char(pm.created_at AT TIME ZONE 'UTC', 'YYYY-MM-DD\"T\"HH24:MI:SS.MS\"Z\"'), "
    "to_char(pm.updated_at AT TIME ZONE 'UTC', 'YYYY-MM-DD\"T\"HH24:MI:SS.MS\"Z\"')";

const char* kProjectAgentColumns =
    "pa.project_id::text, pa.agent_id::text, pa.added_by_user_id::text, "
    "to_char(pa.created_at AT TIME ZONE 'UTC', 'YYYY-MM-DD\"T\"HH24:MI:SS.MS\"Z\"'), "
    "a.name, a.avatar, a.system_prompt, a.status, "
    "to_char(a.created_at AT TIME ZONE 'UTC', 'YYYY-MM-DD\"T\"HH24:MI:SS.MS\"Z\"'), "
    "p.id::text, p.name, p.provider_type, p.model_name";

Json project_json(PgResult& r, int row)
{
    return {
        {"id", r.get(row, 0)}, {"name", r.get(row, 1)},
        {"description", r.is_null(row, 2) ? Json(nullptr) : Json(r.get(row, 2))},
        {"status", r.get(row, 3)}, {"created_by_user_id", r.get(row, 4)},
        {"created_at", r.get(row, 5)}, {"updated_at", r.get(row, 6)}};
}

Json project_member_json(PgResult& r, int row)
{
    return {
        {"project_id", r.get(row, 0)}, {"user_id", r.get(row, 1)}, {"username", r.get(row, 2)},
        {"display_name", r.get(row, 3)}, {"role", r.get(row, 4)},
        {"added_by_user_id", r.is_null(row, 5) ? Json(nullptr) : Json(r.get(row, 5))},
        {"created_at", r.get(row, 6)}, {"updated_at", r.get(row, 7)}};
}

Json project_agent_json(PgResult& r, int row)
{
    return {
        {"project_id", r.get(row, 0)}, {"agent_id", r.get(row, 1)}, {"added_by_user_id", r.get(row, 2)},
        {"created_at", r.get(row, 3)},
        {"agent", {{"id", r.get(row, 1)}, {"name", r.get(row, 4)},
                   {"avatar", r.is_null(row, 5) ? Json(nullptr) : Json(r.get(row, 5))},
                   {"system_prompt", r.get(row, 6)}, {"status", r.get(row, 7)}, {"created_at", r.get(row, 8)},
                   {"model_provider", {{"id", r.get(row, 9)}, {"name", r.get(row, 10)},
                                       {"provider_type", r.get(row, 11)}, {"model_name", r.get(row, 12)}}}}}};
}

const char* kConversationColumns =
    "c.id::text, c.project_id::text, c.owner_id::text, c.agent_id::text, a.name, c.title, c.status, "
    "to_char(c.created_at AT TIME ZONE 'UTC', 'YYYY-MM-DD\"T\"HH24:MI:SS.MS\"Z\"'), "
    "to_char(c.updated_at AT TIME ZONE 'UTC', 'YYYY-MM-DD\"T\"HH24:MI:SS.MS\"Z\"')";

Json conversation_json(PgResult& r, int row)
{
    return {{"id", r.get(row, 0)}, {"project_id", r.get(row, 1)}, {"owner_id", r.get(row, 2)},
            {"agent_id", r.get(row, 3)}, {"agent_name", r.get(row, 4)}, {"title", r.get(row, 5)},
            {"status", r.get(row, 6)}, {"created_at", r.get(row, 7)}, {"updated_at", r.get(row, 8)}};
}

void require_project_role_value(const std::string& role)
{
    if (role != "owner" && role != "editor" && role != "viewer")
        throw ServiceError(400, "VALIDATION_FAILED", "role must be owner, editor, or viewer");
}

std::string ensure_legacy_project(PgSession& s, const std::string& user_id, const std::string& agent_id)
{
    const std::string project_id = s.query(
        "SELECT md5('ai-workspace-v2:legacy-project:' || $1::text)::uuid", {user_id}).get(0, 0);
    s.query(
        "INSERT INTO projects(id,name,description,status,created_by_user_id) "
        "SELECT md5('ai-workspace-v2:legacy-project:' || u.id::text)::uuid, "
        "left('Legacy project - ' || COALESCE(NULLIF(btrim(u.display_name), ''), u.username), 120), "
        "'Auto-created for legacy single-user conversation compatibility.', 'active', u.id "
        "FROM users u WHERE u.id=$1 ON CONFLICT (id) DO NOTHING",
        {user_id});
    s.query(
        "INSERT INTO project_members(project_id,user_id,role,added_by_user_id) VALUES($1,$2,'owner',$2) "
        "ON CONFLICT (project_id,user_id) DO NOTHING",
        {project_id, user_id});
    s.query(
        "INSERT INTO project_agents(project_id,agent_id,added_by_user_id) VALUES($1,$2,$3) "
        "ON CONFLICT (project_id,agent_id) DO NOTHING",
        {project_id, agent_id, user_id});
    return project_id;
}

} // namespace

CoreService::CoreService(Config config) : config_(std::move(config)), db_(config_.database_url) {}

void CoreService::require_uuid(const std::string& value, const std::string& field)
{
    bool valid = value.size() == 36;
    for (std::size_t i = 0; valid && i < value.size(); ++i) {
        if (i == 8 || i == 13 || i == 18 || i == 23) valid = value[i] == '-';
        else valid = std::isxdigit(static_cast<unsigned char>(value[i])) != 0;
    }
    if (!valid) throw ServiceError(400, "VALIDATION_FAILED", field + " must be a UUID");
}

void CoreService::require_text(const std::string& value, const std::string& field, std::size_t max_length)
{
    if (trim(value).empty()) throw ServiceError(400, "VALIDATION_FAILED", field + " is required");
    if (utf8_codepoint_length(value) > max_length) throw ServiceError(400, "VALIDATION_FAILED", field + " is too long");
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

void CoreService::auditDenied(const std::string& user_id, const std::string& attempted_action,
                              const std::string& project_id, const std::string& request_id, const std::string& role)
{
    auto session = db_.session();
    Json metadata = {{"attempted_action", attempted_action}};
    if (!role.empty()) metadata["role"] = role;
    audit(session, "user", user_id, "permission.denied", "project", project_id, "denied", request_id, metadata);
}

std::string CoreService::requireProjectRole(PgSession& session, const std::string& project_id,
                                            const std::string& user_id,
                                            std::initializer_list<std::string_view> allowed, bool lock_project,
                                            const std::string& attempted_action, const std::string& request_id)
{
    auto r = lock_project
                 ? session.query(
                       "SELECT m.role FROM projects p JOIN project_members m ON m.project_id=p.id "
                       "WHERE p.id=$1 AND m.user_id=$2 FOR UPDATE OF p,m",
                       {project_id, user_id})
                 : session.query("SELECT role FROM project_members WHERE project_id=$1 AND user_id=$2",
                                 {project_id, user_id});
    if (r.rows() != 1) {
        auditDenied(user_id, attempted_action, project_id, request_id);
        throw ServiceError(403, "FORBIDDEN", "project is not accessible");
    }
    const std::string role = r.get(0, 0);
    const bool permitted = std::any_of(allowed.begin(), allowed.end(), [&](std::string_view candidate) {
        return candidate == role;
    });
    if (!permitted) {
        auditDenied(user_id, attempted_action, project_id, request_id, role);
        throw ServiceError(403, "FORBIDDEN", "project role does not permit this action");
    }
    return role;
}

std::string CoreService::requireProjectAgent(PgSession& session, const std::string& project_id,
                                             const std::string& agent_id, const std::string& user_id,
                                             const std::string& attempted_action, const std::string& request_id)
{
    auto r = session.query(
        "SELECT a.model_provider_id::text FROM project_agents pa "
        "JOIN agents a ON a.id=pa.agent_id "
        "JOIN project_members owner_pm ON owner_pm.project_id=pa.project_id AND owner_pm.user_id=a.owner_id "
        "WHERE pa.project_id=$1 AND pa.agent_id=$2 AND a.status='active'",
        {project_id, agent_id});
    if (r.rows() != 1) {
        auditDenied(user_id, attempted_action, project_id, request_id);
        throw ServiceError(403, "FORBIDDEN", "target agent is not active in this project");
    }
    return r.get(0, 0);
}

Json CoreService::login(const std::string& username, const std::string& password, const std::string& request_id)
{
    require_text(username, "username", 80);
    require_text(password, "password", 200);
    if (utf8_codepoint_length(password) < 8) throw ServiceError(400, "VALIDATION_FAILED", "password must be at least 8 characters");
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
    require_allowed_fields(body, {"name", "avatar", "system_prompt", "model_provider_id"});
    const std::string name = json_string(body, "name");
    const std::string system_prompt = json_string(body, "system_prompt");
    require_text(name, "name", 80);
    if (utf8_codepoint_length(system_prompt) > 8000) throw ServiceError(400, "VALIDATION_FAILED", "system_prompt is too long");
    if (body.contains("avatar") && !body["avatar"].is_null() && !body["avatar"].is_string())
        throw ServiceError(400, "VALIDATION_FAILED", "avatar must be a string or null");
    const std::string avatar = body.contains("avatar") && body["avatar"].is_string() ? body["avatar"].get<std::string>() : "";
    if (utf8_codepoint_length(avatar) > 500000) throw ServiceError(400, "VALIDATION_FAILED", "avatar is too long");
    auto s = db_.session();
    PgResult provider;
    if (body.contains("model_provider_id") && !body["model_provider_id"].is_null()) {
        const std::string provider_id = json_string(body, "model_provider_id");
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

Json CoreService::listProjects(const std::string& user_id, const std::string&)
{
    auto s = db_.session();
    auto r = s.query(
        std::string("SELECT ") + kProjectColumns +
            " FROM projects p JOIN project_members m ON m.project_id=p.id "
            "WHERE m.user_id=$1 ORDER BY p.updated_at DESC, p.id",
        {user_id});
    Json items = Json::array();
    for (int row = 0; row < r.rows(); ++row) items.push_back(project_json(r, row));
    return {{"items", items}};
}

Json CoreService::createProject(const std::string& user_id, const Json& body, const std::string& request_id)
{
    require_allowed_fields(body, {"name", "description"});
    const std::string name = json_string(body, "name");
    require_text(name, "name", 120);
    Json description = nullptr;
    if (body.contains("description") && !body["description"].is_null()) {
        if (!body["description"].is_string())
            throw ServiceError(400, "VALIDATION_FAILED", "description must be a string or null");
        description = body["description"].get<std::string>();
        if (utf8_codepoint_length(description.get<std::string>()) > 20000)
            throw ServiceError(400, "VALIDATION_FAILED", "description is too long");
    }

    const std::string project_id = uuid4();
    auto s = db_.session();
    s.begin();
    try {
        s.query("INSERT INTO projects(id,name,description,created_by_user_id) VALUES($1,$2,$3,$4)",
                {project_id, name,
                 description.is_null() ? std::nullopt : std::optional<std::string>(description.get<std::string>()),
                 user_id});
        s.query("INSERT INTO project_members(project_id,user_id,role,added_by_user_id) VALUES($1,$2,'owner',$2)",
                {project_id, user_id});
        audit(s, "user", user_id, "project.create", "project", project_id, "success", request_id);
        auto project = s.query(std::string("SELECT ") + kProjectColumns + " FROM projects p WHERE p.id=$1",
                               {project_id});
        s.commit();
        return project_json(project, 0);
    } catch (...) {
        s.rollback();
        throw;
    }
}

Json CoreService::getProject(const std::string& user_id, const std::string& project_id,
                             const std::string& request_id)
{
    require_uuid(project_id, "project_id");
    auto s = db_.session();
    requireProjectRole(s, project_id, user_id, {"owner", "editor", "viewer"}, false, "project.read", request_id);
    auto r = s.query(std::string("SELECT ") + kProjectColumns + " FROM projects p WHERE p.id=$1", {project_id});
    if (r.rows() != 1) throw ServiceError(404, "NOT_FOUND", "project not found");
    return project_json(r, 0);
}

Json CoreService::listProjectMembers(const std::string& user_id, const std::string& project_id,
                                     const std::string& request_id)
{
    require_uuid(project_id, "project_id");
    auto s = db_.session();
    requireProjectRole(s, project_id, user_id, {"owner", "editor", "viewer"}, false,
                       "project.member.list", request_id);
    auto r = s.query(
        std::string("SELECT ") + kProjectMemberColumns +
            " FROM project_members pm JOIN users u ON u.id=pm.user_id "
            "WHERE pm.project_id=$1 ORDER BY CASE pm.role WHEN 'owner' THEN 0 WHEN 'editor' THEN 1 ELSE 2 END, "
            "pm.created_at, pm.user_id",
        {project_id});
    Json items = Json::array();
    for (int row = 0; row < r.rows(); ++row) items.push_back(project_member_json(r, row));
    return {{"items", items}};
}

Json CoreService::addProjectMember(const std::string& user_id, const std::string& project_id, const Json& body,
                                   const std::string& request_id)
{
    require_allowed_fields(body, {"user_id", "role"});
    const std::string target_user_id = json_string(body, "user_id");
    const std::string role = json_string(body, "role");
    require_uuid(project_id, "project_id");
    require_uuid(target_user_id, "user_id");
    require_project_role_value(role);

    auto s = db_.session();
    s.begin();
    try {
        requireProjectRole(s, project_id, user_id, {"owner"}, true, "project.member.add", request_id);
        auto target = s.query("SELECT id::text FROM users WHERE id=$1 AND status='active'", {target_user_id});
        if (target.rows() != 1) throw ServiceError(404, "NOT_FOUND", "user not found");
        auto inserted = s.query(
            "INSERT INTO project_members(project_id,user_id,role,added_by_user_id) VALUES($1,$2,$3,$4) "
            "ON CONFLICT (project_id,user_id) DO NOTHING RETURNING user_id::text",
            {project_id, target_user_id, role, user_id});
        if (inserted.rows() == 0) throw ServiceError(409, "CONFLICT", "user is already a project member");
        audit(s, "user", user_id, "project.member.add", "project", project_id, "success", request_id,
              {{"member_user_id", target_user_id}, {"role", role}});
        auto member = s.query(
            std::string("SELECT ") + kProjectMemberColumns +
                " FROM project_members pm JOIN users u ON u.id=pm.user_id WHERE pm.project_id=$1 AND pm.user_id=$2",
            {project_id, target_user_id});
        s.commit();
        return project_member_json(member, 0);
    } catch (...) {
        s.rollback();
        throw;
    }
}

Json CoreService::updateProjectMember(const std::string& user_id, const std::string& project_id,
                                      const std::string& target_user_id, const Json& body,
                                      const std::string& request_id)
{
    require_allowed_fields(body, {"role"});
    const std::string role = json_string(body, "role");
    require_uuid(project_id, "project_id");
    require_uuid(target_user_id, "user_id");
    require_project_role_value(role);

    auto s = db_.session();
    s.begin();
    try {
        requireProjectRole(s, project_id, user_id, {"owner"}, true, "project.member.role_change", request_id);
        auto target = s.query(
            "SELECT pm.role,u.username,u.display_name FROM project_members pm JOIN users u ON u.id=pm.user_id "
            "WHERE pm.project_id=$1 AND pm.user_id=$2 FOR UPDATE OF pm",
            {project_id, target_user_id});
        if (target.rows() != 1) throw ServiceError(404, "NOT_FOUND", "project member not found");
        const std::string current_role = target.get(0, 0);
        if (current_role == "owner" && role != "owner") {
            auto owners = s.query("SELECT count(*)::text FROM project_members WHERE project_id=$1 AND role='owner'",
                                  {project_id});
            if (std::stoi(owners.get(0, 0)) <= 1)
                throw ServiceError(409, "CONFLICT", "cannot downgrade the last project owner");
        }
        s.query("UPDATE project_members SET role=$1,updated_at=now() WHERE project_id=$2 AND user_id=$3",
                {role, project_id, target_user_id});
        audit(s, "user", user_id, "project.member.role_change", "project", project_id, "success", request_id,
              {{"member_user_id", target_user_id}, {"from_role", current_role}, {"to_role", role}});
        auto member = s.query(
            std::string("SELECT ") + kProjectMemberColumns +
                " FROM project_members pm JOIN users u ON u.id=pm.user_id WHERE pm.project_id=$1 AND pm.user_id=$2",
            {project_id, target_user_id});
        s.commit();
        return project_member_json(member, 0);
    } catch (...) {
        s.rollback();
        throw;
    }
}

void CoreService::removeProjectMember(const std::string& user_id, const std::string& project_id,
                                      const std::string& target_user_id, const std::string& request_id)
{
    require_uuid(project_id, "project_id");
    require_uuid(target_user_id, "user_id");
    auto s = db_.session();
    s.begin();
    try {
        requireProjectRole(s, project_id, user_id, {"owner"}, true, "project.member.remove", request_id);
        auto target = s.query(
            "SELECT pm.role FROM project_members pm WHERE pm.project_id=$1 AND pm.user_id=$2 FOR UPDATE OF pm",
            {project_id, target_user_id});
        if (target.rows() != 1) throw ServiceError(404, "NOT_FOUND", "project member not found");
        if (target.get(0, 0) == "owner") {
            auto owners = s.query("SELECT count(*)::text FROM project_members WHERE project_id=$1 AND role='owner'",
                                  {project_id});
            if (std::stoi(owners.get(0, 0)) <= 1)
                throw ServiceError(409, "CONFLICT", "cannot remove the last project owner");
        }
        auto active = s.query(
            "SELECT 1 FROM runs r JOIN conversations c ON c.id=r.conversation_id "
            "JOIN agents a ON a.id=r.agent_id "
            "WHERE c.project_id=$1 AND (r.initiated_by_user_id=$2 OR a.owner_id=$2) "
            "AND r.status IN ('queued','running') LIMIT 1",
            {project_id, target_user_id});
        if (active.rows() == 1)
            throw ServiceError(409, "CONFLICT", "member has an active run in this project");
        s.query("DELETE FROM project_members WHERE project_id=$1 AND user_id=$2", {project_id, target_user_id});
        audit(s, "user", user_id, "project.member.remove", "project", project_id, "success", request_id,
              {{"member_user_id", target_user_id}});
        s.commit();
    } catch (...) {
        s.rollback();
        throw;
    }
}

Json CoreService::listProjectAgents(const std::string& user_id, const std::string& project_id,
                                    const std::string& request_id)
{
    require_uuid(project_id, "project_id");
    auto s = db_.session();
    requireProjectRole(s, project_id, user_id, {"owner", "editor", "viewer"}, false,
                       "project.agent.list", request_id);
    auto r = s.query(
        std::string("SELECT ") + kProjectAgentColumns +
            " FROM project_agents pa JOIN agents a ON a.id=pa.agent_id "
            "JOIN model_providers p ON p.id=a.model_provider_id "
            "WHERE pa.project_id=$1 ORDER BY pa.created_at, pa.agent_id",
        {project_id});
    Json items = Json::array();
    for (int row = 0; row < r.rows(); ++row) items.push_back(project_agent_json(r, row));
    return {{"items", items}};
}

Json CoreService::addProjectAgent(const std::string& user_id, const std::string& project_id, const Json& body,
                                  const std::string& request_id)
{
    require_allowed_fields(body, {"agent_id"});
    const std::string agent_id = json_string(body, "agent_id");
    require_uuid(project_id, "project_id");
    require_uuid(agent_id, "agent_id");

    auto s = db_.session();
    s.begin();
    try {
        const std::string role = requireProjectRole(s, project_id, user_id, {"owner", "editor"}, true,
                                                     "project.agent.add", request_id);
        auto agent = s.query("SELECT owner_id::text,status FROM agents WHERE id=$1", {agent_id});
        if (agent.rows() != 1 || agent.get(0, 0) != user_id || agent.get(0, 1) != "active") {
            auditDenied(user_id, "project.agent.add", project_id, request_id, role);
            throw ServiceError(403, "FORBIDDEN", "only the active owner may add this agent");
        }
        auto inserted = s.query(
            "INSERT INTO project_agents(project_id,agent_id,added_by_user_id) VALUES($1,$2,$3) "
            "ON CONFLICT (project_id,agent_id) DO NOTHING RETURNING agent_id::text",
            {project_id, agent_id, user_id});
        if (inserted.rows() == 0) throw ServiceError(409, "CONFLICT", "agent is already in this project");
        audit(s, "user", user_id, "project.agent.add", "project", project_id, "success", request_id,
              {{"agent_id", agent_id}});
        auto project_agent = s.query(
            std::string("SELECT ") + kProjectAgentColumns +
                " FROM project_agents pa JOIN agents a ON a.id=pa.agent_id "
                "JOIN model_providers p ON p.id=a.model_provider_id WHERE pa.project_id=$1 AND pa.agent_id=$2",
            {project_id, agent_id});
        s.commit();
        return project_agent_json(project_agent, 0);
    } catch (...) {
        s.rollback();
        throw;
    }
}

void CoreService::removeProjectAgent(const std::string& user_id, const std::string& project_id,
                                     const std::string& agent_id, const std::string& request_id)
{
    require_uuid(project_id, "project_id");
    require_uuid(agent_id, "agent_id");
    auto s = db_.session();
    s.begin();
    try {
        const std::string role = requireProjectRole(s, project_id, user_id, {"owner", "editor"}, true,
                                                     "project.agent.remove", request_id);
        auto agent = s.query(
            "SELECT a.owner_id::text FROM project_agents pa JOIN agents a ON a.id=pa.agent_id "
            "WHERE pa.project_id=$1 AND pa.agent_id=$2 FOR UPDATE OF pa",
            {project_id, agent_id});
        if (agent.rows() != 1) throw ServiceError(404, "NOT_FOUND", "project agent not found");
        if (role == "editor" && agent.get(0, 0) != user_id) {
            auditDenied(user_id, "project.agent.remove", project_id, request_id, role);
            throw ServiceError(403, "FORBIDDEN", "editors may only remove their own agents");
        }
        auto active = s.query(
            "SELECT 1 FROM runs r JOIN conversations c ON c.id=r.conversation_id "
            "WHERE c.project_id=$1 AND r.agent_id=$2 AND r.status IN ('queued','running') LIMIT 1",
            {project_id, agent_id});
        if (active.rows() == 1)
            throw ServiceError(409, "CONFLICT", "agent has an active run in this project");
        s.query("DELETE FROM project_agents WHERE project_id=$1 AND agent_id=$2", {project_id, agent_id});
        audit(s, "user", user_id, "project.agent.remove", "project", project_id, "success", request_id,
              {{"agent_id", agent_id}});
        s.commit();
    } catch (...) {
        s.rollback();
        throw;
    }
}

Json CoreService::listConversations(const std::string& user_id, const std::string&)
{
    auto s = db_.session();
    auto r = s.query(
        std::string("SELECT ") + kConversationColumns +
            " FROM conversations c JOIN agents a ON a.id=c.agent_id "
            "JOIN project_members pm ON pm.project_id=c.project_id "
            "WHERE pm.user_id=$1 ORDER BY c.updated_at DESC, c.id",
        {user_id});
    Json items = Json::array();
    for (int row = 0; row < r.rows(); ++row) items.push_back(conversation_json(r, row));
    return {{"items", items}};
}

Json CoreService::listProjectConversations(const std::string& user_id, const std::string& project_id,
                                           const std::string& request_id)
{
    require_uuid(project_id, "project_id");
    auto s = db_.session();
    requireProjectRole(s, project_id, user_id, {"owner", "editor", "viewer"}, false,
                       "conversation.list", request_id);
    auto r = s.query(
        std::string("SELECT ") + kConversationColumns +
            " FROM conversations c JOIN agents a ON a.id=c.agent_id "
            "WHERE c.project_id=$1 ORDER BY c.updated_at DESC, c.id",
        {project_id});
    Json items = Json::array();
    for (int row = 0; row < r.rows(); ++row) items.push_back(conversation_json(r, row));
    return {{"items", items}};
}

Json CoreService::createConversation(const std::string& user_id, const Json& body, const std::string& request_id)
{
    require_allowed_fields(body, {"agent_id", "title"});
    const std::string agent_id = json_string(body, "agent_id");
    require_uuid(agent_id, "agent_id");
    std::string title = json_string(body, "title");
    if (trim(title).empty()) title = "New conversation";
    require_text(title, "title", 200);

    auto s = db_.session();
    auto agent = s.query("SELECT id::text FROM agents WHERE id=$1 AND owner_id=$2 AND status='active'",
                         {agent_id, user_id});
    if (agent.rows() != 1) throw ServiceError(403, "FORBIDDEN", "agent is not accessible");
    const std::string id = uuid4();
    s.begin();
    try {
        const std::string project_id = ensure_legacy_project(s, user_id, agent_id);
        s.query(
            "INSERT INTO conversations(id,owner_id,agent_id,project_id,title) VALUES($1,$2,$3,$4,$5)",
            {id, user_id, agent_id, project_id, title});
        auto conversation = s.query(
            std::string("SELECT ") + kConversationColumns +
                " FROM conversations c JOIN agents a ON a.id=c.agent_id WHERE c.id=$1",
            {id});
        audit(s, "user", user_id, "conversation.create", "conversation", id, "success", request_id,
              {{"project_id", project_id}, {"agent_id", agent_id}});
        s.commit();
        return conversation_json(conversation, 0);
    } catch (...) {
        s.rollback();
        throw;
    }
}

Json CoreService::createProjectConversation(const std::string& user_id, const std::string& project_id,
                                            const Json& body, const std::string& request_id)
{
    require_allowed_fields(body, {"agent_id", "title"});
    const std::string agent_id = json_string(body, "agent_id");
    require_uuid(project_id, "project_id");
    require_uuid(agent_id, "agent_id");
    std::string title = json_string(body, "title");
    if (trim(title).empty()) title = "New conversation";
    require_text(title, "title", 200);

    auto s = db_.session();
    s.begin();
    try {
        const std::string role = requireProjectRole(s, project_id, user_id, {"owner", "editor"}, true,
                                                     "conversation.create", request_id);
        auto project = s.query("SELECT status FROM projects WHERE id=$1", {project_id});
        if (project.rows() != 1 || project.get(0, 0) != "active")
            throw ServiceError(409, "CONFLICT", "project is archived");
        (void)requireProjectAgent(s, project_id, agent_id, user_id, "conversation.create", request_id);

        const std::string id = uuid4();
        s.query(
            "INSERT INTO conversations(id,owner_id,agent_id,project_id,title) VALUES($1,$2,$3,$4,$5)",
            {id, user_id, agent_id, project_id, title});
        auto conversation = s.query(
            std::string("SELECT ") + kConversationColumns +
                " FROM conversations c JOIN agents a ON a.id=c.agent_id WHERE c.id=$1",
            {id});
        audit(s, "user", user_id, "conversation.create", "conversation", id, "success", request_id,
              {{"project_id", project_id}, {"agent_id", agent_id}, {"role", role}});
        s.commit();
        return conversation_json(conversation, 0);
    } catch (...) {
        s.rollback();
        throw;
    }
}

Json CoreService::listMessages(const std::string& user_id, const std::string& conversation_id,
                               long long after_seq, const std::string& request_id)
{
    require_uuid(conversation_id, "conversation_id");
    auto s = db_.session();
    auto conversation = s.query("SELECT project_id::text FROM conversations WHERE id=$1", {conversation_id});
    if (conversation.rows() != 1) throw ServiceError(403, "FORBIDDEN", "conversation is not accessible");
    requireProjectRole(s, conversation.get(0, 0), user_id, {"owner", "editor", "viewer"}, false,
                       "message.list", request_id);
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
    require_allowed_fields(body, {"content", "client_request_id", "target_agent_id"});
    require_uuid(conversation_id, "conversation_id");
    const std::string content = json_string(body, "content");
    const std::string target_agent_id = json_string(body, "target_agent_id");
    require_text(content, "content", 32000);
    require_uuid(target_agent_id, "target_agent_id");
    if (client_request_id.empty())
        throw ServiceError(400, "VALIDATION_FAILED", "client_request_id is required");
    if (utf8_codepoint_length(client_request_id) < 8 || utf8_codepoint_length(client_request_id) > 100)
        throw ServiceError(400, "VALIDATION_FAILED", "client_request_id must be 8-100 characters");

    auto s = db_.session();
    s.begin();
    try {
        const std::string duplicate_sql = std::string("SELECT ") + kMessageColumns +
            ",r.id::text,r.status FROM messages m JOIN runs r ON r.id=m.run_id "
            "WHERE m.conversation_id=$1 AND m.sender_id=$2 AND m.client_request_id=$3 "
            "AND m.sender_type='user' LIMIT 1";
        auto duplicate_json = [](PgResult& existing) {
            return Json{{"message", message_json(existing, 0)}, {"run_id", existing.get(0, 10)},
                        {"status", existing.get(0, 11)},
                        {"events_url", "/api/v1/runs/" + existing.get(0, 10) + "/events"},
                        {"duplicate", true}};
        };

        auto conversation_project = s.query("SELECT project_id::text FROM conversations WHERE id=$1",
                                            {conversation_id});
        if (conversation_project.rows() != 1)
            throw ServiceError(403, "FORBIDDEN", "conversation is not accessible");
        const std::string project_id = conversation_project.get(0, 0);
        // ponytail: project row lock serializes conversation writes with member/agent removal;
        // narrow to per-conversation locks if project-level write throughput becomes a problem.
        const std::string role = requireProjectRole(s, project_id, user_id, {"owner", "editor"}, true,
                                                     "message.send", request_id);
        auto project = s.query("SELECT status FROM projects WHERE id=$1", {project_id});
        if (project.rows() != 1 || project.get(0, 0) != "active")
            throw ServiceError(409, "CONFLICT", "project is archived");
        auto conversation = s.query(
            "SELECT status FROM conversations WHERE id=$1 AND project_id=$2 FOR UPDATE",
            {conversation_id, project_id});
        if (conversation.rows() != 1)
            throw ServiceError(403, "FORBIDDEN", "conversation is not accessible");
        if (conversation.get(0, 0) != "active")
            throw ServiceError(409, "CONFLICT", "conversation is archived");

        auto existing = s.query(duplicate_sql, {conversation_id, user_id, client_request_id});
        if (existing.rows() == 1) {
            s.commit();
            return duplicate_json(existing);
        }

        const std::string provider_id = requireProjectAgent(
            s, project_id, target_agent_id, user_id, "message.send", request_id);
        auto active = s.query(
            "SELECT 1 FROM runs WHERE conversation_id=$1 AND status IN ('queued','running') LIMIT 1",
            {conversation_id});
        if (active.rows() == 1)
            throw ServiceError(409, "CONFLICT", "conversation already has an active run");

        const std::string message_id = uuid4();
        const std::string run_id = uuid4();
        const std::string job_id = uuid4();
        auto msg = s.query(
            "INSERT INTO messages(id,conversation_id,sender_type,sender_id,role,content,status,client_request_id) "
            "VALUES($1,$2,'user',$3,'user',$4,'completed',$5) "
            "ON CONFLICT (conversation_id,sender_id,client_request_id) "
            "WHERE sender_type='user' AND sender_id IS NOT NULL AND client_request_id IS NOT NULL "
            "DO NOTHING RETURNING id::text,conversation_id::text,seq::text,"
            "sender_type,sender_id::text,role,content,run_id::text,status,"
            "to_char(created_at AT TIME ZONE 'UTC','YYYY-MM-DD\"T\"HH24:MI:SS.MS\"Z\"')",
            {message_id, conversation_id, user_id, content, client_request_id});
        if (msg.rows() == 0) {
            auto existing = s.query(duplicate_sql, {conversation_id, user_id, client_request_id});
            if (existing.rows() != 1)
                throw ServiceError(500, "INTERNAL", "duplicate message is unavailable", true);
            s.commit();
            return duplicate_json(existing);
        }
        s.query("INSERT INTO runs(id,conversation_id,agent_id,trigger_message_id,model_provider_id,status,initiated_by_user_id) "
                "VALUES($1,$2,$3,$4,$5,'queued',$6)",
                {run_id, conversation_id, target_agent_id, message_id, provider_id, user_id});
        s.query("UPDATE messages SET run_id=$1 WHERE id=$2", {run_id, message_id});
        s.query("INSERT INTO jobs(id,run_id,status) VALUES($1,$2,'queued')", {job_id, run_id});
        s.query("UPDATE conversations SET updated_at=now() WHERE id=$1", {conversation_id});
        audit(s, "user", user_id, "message.send", "run", run_id, "success", request_id,
              {{"conversation_id", conversation_id}, {"agent_id", target_agent_id}, {"role", role},
               {"initiated_by_user_id", user_id}});
        audit(s, "user", user_id, "run.start", "run", run_id, "success", request_id,
              {{"conversation_id", conversation_id}, {"agent_id", target_agent_id},
               {"initiated_by_user_id", user_id}});
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

Json CoreService::retryRun(const std::string& user_id, const std::string& run_id,
                            const std::string& idempotency_key, const std::string& request_id)
{
    require_uuid(run_id, "run_id");
    if (!idempotency_key.empty() && (utf8_codepoint_length(idempotency_key) < 8 || utf8_codepoint_length(idempotency_key) > 100))
        throw ServiceError(400, "VALIDATION_FAILED", "Idempotency-Key must be 8-100 characters");

    auto s = db_.session();
    s.begin();
    try {
        auto initial = s.query(
            "SELECT c.project_id::text,c.id::text,r.initiated_by_user_id::text FROM runs r "
            "JOIN conversations c ON c.id=r.conversation_id WHERE r.id=$1",
            {run_id});
        if (initial.rows() != 1)
            throw ServiceError(403, "FORBIDDEN", "run is not accessible");
        const std::string project_id = initial.get(0, 0);
        const std::string conversation_id = initial.get(0, 1);
        const std::string role = requireProjectRole(s, project_id, user_id, {"owner", "editor"}, true,
                                                     "run.retry", request_id);
        auto project = s.query("SELECT status FROM projects WHERE id=$1", {project_id});
        if (project.rows() != 1 || project.get(0, 0) != "active")
            throw ServiceError(409, "CONFLICT", "project is archived");
        auto conversation = s.query(
            "SELECT status FROM conversations WHERE id=$1 AND project_id=$2 FOR UPDATE",
            {conversation_id, project_id});
        if (conversation.rows() != 1)
            throw ServiceError(403, "FORBIDDEN", "conversation is not accessible");
        if (conversation.get(0, 0) != "active")
            throw ServiceError(409, "CONFLICT", "conversation is archived");

        auto old = s.query(
            "SELECT r.agent_id::text,r.model_provider_id::text,r.trigger_message_id::text,r.status,"
            "r.initiated_by_user_id::text FROM runs r "
            "WHERE r.id=$1 AND r.conversation_id=$2 FOR UPDATE OF r",
            {run_id, conversation_id});
        if (old.rows() != 1)
            throw ServiceError(403, "FORBIDDEN", "run is not accessible");
        if (role == "editor" && old.get(0, 4) != user_id) {
            auditDenied(user_id, "run.retry", project_id, request_id, role);
            throw ServiceError(403, "FORBIDDEN", "editors may only retry their own runs");
        }

        auto duplicate_response = [&](PgSession& session) {
            auto existing = session.query(
                "SELECT r.id::text,r.status,r.trigger_message_id::text FROM retry_idempotency i "
                "JOIN runs r ON r.id=i.run_id "
                "WHERE i.user_id=$1 AND i.operation='retry_run' AND i.key=$2",
                {user_id, idempotency_key});
            if (existing.rows() != 1)
                throw ServiceError(409, "CONFLICT", "idempotency key is already in use");
            auto msg = session.query(std::string("SELECT ") + kMessageColumns +
                                         " FROM messages m WHERE m.id=$1",
                                     {existing.get(0, 2)});
            if (msg.rows() != 1)
                throw ServiceError(500, "INTERNAL", "retry source message is unavailable", true);
            Json message = message_json(msg, 0);
            message["run_id"] = existing.get(0, 0);
            return Json{{"message", message}, {"run_id", existing.get(0, 0)}, {"status", existing.get(0, 1)},
                        {"events_url", "/api/v1/runs/" + existing.get(0, 0) + "/events"},
                        {"duplicate", true}};
        };

        if (!idempotency_key.empty()) {
            auto existing = s.query(
                "SELECT 1 FROM retry_idempotency WHERE user_id=$1 AND operation='retry_run' AND key=$2",
                {user_id, idempotency_key});
            if (existing.rows() == 1) {
                auto response = duplicate_response(s);
                s.commit();
                return response;
            }
        }
        if (old.get(0, 3) != "failed" && old.get(0, 3) != "cancelled")
            throw ServiceError(409, "CONFLICT", "only failed or cancelled runs can be retried");

        const std::string provider_id = requireProjectAgent(
            s, project_id, old.get(0, 0), user_id, "run.retry", request_id);
        auto active = s.query(
            "SELECT 1 FROM runs WHERE conversation_id=$1 AND status IN ('queued','running') LIMIT 1",
            {conversation_id});
        if (active.rows() == 1)
            throw ServiceError(409, "CONFLICT", "conversation already has an active run");

        const std::string new_run_id = uuid4();
        const std::string job_id = uuid4();
        s.query("INSERT INTO runs(id,conversation_id,agent_id,trigger_message_id,model_provider_id,status,initiated_by_user_id) "
                "VALUES($1,$2,$3,$4,$5,'queued',$6)",
                {new_run_id, conversation_id, old.get(0, 0), old.get(0, 2), provider_id, user_id});
        s.query("UPDATE messages SET run_id=$1 WHERE id=$2", {new_run_id, old.get(0, 2)});
        s.query("INSERT INTO jobs(id,run_id,status) VALUES($1,$2,'queued')", {job_id, new_run_id});
        if (!idempotency_key.empty()) {
            auto claimed = s.query(
                "INSERT INTO retry_idempotency(user_id,operation,key,run_id,source_run_id) "
                "VALUES($1,'retry_run',$2,$3,$4) ON CONFLICT (user_id,operation,key) DO NOTHING "
                "RETURNING run_id::text",
                {user_id, idempotency_key, new_run_id, run_id});
            if (claimed.rows() == 0) {
                s.rollback();
                return duplicate_response(s);
            }
        }
        s.query("UPDATE conversations SET updated_at=now() WHERE id=$1", {conversation_id});
        audit(s, "user", user_id, "run.retry", "run", new_run_id, "success", request_id,
              {{"source_run_id", run_id}, {"conversation_id", conversation_id},
               {"agent_id", old.get(0, 0)}, {"role", role}, {"initiated_by_user_id", user_id}});
        auto msg = s.query(std::string("SELECT ") + kMessageColumns + " FROM messages m WHERE m.id=$1",
                           {old.get(0, 2)});
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
        auto access = s.query(
            "SELECT c.project_id::text FROM runs r JOIN conversations c ON c.id=r.conversation_id "
            "WHERE r.id=$1",
            {run_id});
        if (access.rows() != 1)
            throw ServiceError(403, "FORBIDDEN", "run is not accessible");
        const std::string project_id = access.get(0, 0);
        const std::string role = requireProjectRole(s, project_id, user_id, {"owner", "editor"}, true,
                                                     "run.cancel", request_id);
        auto r = s.query(
            std::string("SELECT ") + kRunColumns +
                ",j.status,r.initiated_by_user_id::text FROM runs r "
                "JOIN conversations c ON c.id=r.conversation_id "
                "JOIN jobs j ON j.run_id=r.id "
                "WHERE r.id=$1 AND c.project_id=$2 FOR UPDATE OF r,j",
            {run_id, project_id});
        if (r.rows() != 1)
            throw ServiceError(403, "FORBIDDEN", "run is not accessible");
        if (role == "editor" && r.get(0, 9) != user_id) {
            auditDenied(user_id, "run.cancel", project_id, request_id, role);
            throw ServiceError(403, "FORBIDDEN", "editors may only cancel their own runs");
        }
        const std::string status = r.get(0, 2);
        if (status == "succeeded" || status == "failed" || status == "cancelled")
            throw ServiceError(409, "RUN_NOT_CANCELLABLE", "run is already terminal");
        if (r.get(0, 8) == "queued") {
            const std::string next_seq = std::to_string(std::stoll(s.query(
                "SELECT COALESCE(MAX(seq),0)+1 FROM run_events WHERE run_id=$1", {run_id}).get(0, 0)));
            s.query("UPDATE jobs SET status='cancelled',lease_token=NULL,lease_until=NULL,updated_at=now() "
                    "WHERE run_id=$1", {run_id});
            s.query("UPDATE runs SET status='cancelled',error_code='RUN_CANCELLED',cancel_requested=true,"
                    "finished_at=now() WHERE id=$1", {run_id});
            s.query("INSERT INTO run_events(run_id,seq,event_type,payload_jsonb,request_id) "
                    "VALUES($1,$2,'run.cancelled',$3::jsonb,$4) ON CONFLICT (run_id,event_type) "
                    "WHERE event_type IN ('run.started','message.completed','run.failed','run.cancelled') "
                    "DO NOTHING",
                    {run_id, next_seq, Json{{"reason", "user_requested"}}.dump(), request_id});
        } else {
            s.query("UPDATE runs SET cancel_requested=true WHERE id=$1", {run_id});
        }
        audit(s, "user", user_id, "run.cancel", "run", run_id, "success", request_id,
              {{"project_id", project_id}, {"role", role}});
        s.commit();
    } catch (...) {
        s.rollback();
        throw;
    }
    return getRun(user_id, run_id, request_id);
}

Json CoreService::getRun(const std::string& user_id, const std::string& run_id,
                         const std::string& request_id)
{
    require_uuid(run_id, "run_id");
    auto s = db_.session();
    auto access = s.query(
        "SELECT c.project_id::text FROM runs r JOIN conversations c ON c.id=r.conversation_id "
        "WHERE r.id=$1",
        {run_id});
    if (access.rows() != 1)
        throw ServiceError(403, "FORBIDDEN", "run is not accessible");
    requireProjectRole(s, access.get(0, 0), user_id, {"owner", "editor", "viewer"}, false,
                       "run.read", request_id);
    auto r = s.query(std::string("SELECT ") + kRunColumns + " FROM runs r WHERE r.id=$1", {run_id});
    if (r.rows() != 1)
        throw ServiceError(403, "FORBIDDEN", "run is not accessible");
    return run_json(r, 0);
}

Json CoreService::listRunEvents(const std::string& user_id, const std::string& run_id, long long after_seq,
                                const std::string& request_id)
{
    require_uuid(run_id, "run_id");
    auto s = db_.session();
    auto access = s.query(
        "SELECT c.project_id::text FROM runs r JOIN conversations c ON c.id=r.conversation_id "
        "WHERE r.id=$1",
        {run_id});
    if (access.rows() != 1)
        throw ServiceError(403, "FORBIDDEN", "run is not accessible");
    requireProjectRole(s, access.get(0, 0), user_id, {"owner", "editor", "viewer"}, false,
                       "run.events", request_id);
    auto r = s.query(
        "SELECT seq::text,event_type,payload_jsonb::text,request_id,"
        "to_char(created_at AT TIME ZONE 'UTC','YYYY-MM-DD\"T\"HH24:MI:SS.MS\"Z\"') "
        "FROM run_events WHERE run_id=$1 AND seq>$2 ORDER BY run_events.seq",
        {run_id, std::to_string(after_seq)});
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
                "COALESCE(j.lease_until<now(),false) AS expired,r.cancel_requested FROM jobs j "
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
            if (r.get(0, 6) == "t") {
                auto next = s.query("SELECT COALESCE(MAX(seq),0)+1 FROM run_events WHERE run_id=$1", {run_id});
                s.query("UPDATE jobs SET status='cancelled',lease_token=NULL,lease_until=NULL,updated_at=now() WHERE id=$1", {job_id});
                s.query("UPDATE runs SET status='cancelled',error_code='RUN_CANCELLED',cancel_requested=true,finished_at=now() WHERE id=$1", {run_id});
                s.query("INSERT INTO run_events(run_id,seq,event_type,payload_jsonb,request_id) VALUES($1,$2,'run.cancelled',$3::jsonb,$4) "
                        "ON CONFLICT (run_id,event_type) WHERE event_type IN ('run.started','message.completed','run.failed','run.cancelled') DO NOTHING",
                        {run_id, next.get(0, 0), Json{{"reason", "user_requested"}}.dump(), request_id});
                audit(s, "worker", worker_id, "job.cancel", "job", job_id, "success", request_id, {{"run_id", run_id}});
                s.commit();
                continue;
            }
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
    if (utf8_codepoint_length(final_content) > 200000)
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
            "SELECT j.status,j.attempt_count,j.max_attempts,r.status,r.cancel_requested,r.agent_id::text,j.lease_token,"
            "COALESCE(j.lease_until>now(),false) "
            "FROM jobs j JOIN runs r ON r.id=j.run_id WHERE j.id=$1 AND r.id=$2 "
            "FOR UPDATE OF j,r", {job_id, run_id});
        if (r.rows() != 1) throw ServiceError(409, "JOB_LEASE_INVALID", "job lease is invalid", true);
        if (r.get(0, 3) == "succeeded" || r.get(0, 3) == "failed" || r.get(0, 3) == "cancelled")
            return {{"accepted", true}, {"duplicate", true}, {"run_status", r.get(0, 3)}, {"final_event_seq", 0}};
        if (r.get(0, 0) != "running" || lease_token.empty() || r.get(0, 6) != lease_token || r.get(0, 7) != "t")
            throw ServiceError(409, "JOB_LEASE_INVALID", "job lease is invalid or expired", true);

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
        r = s.query(
            "SELECT pm.role FROM agents a JOIN project_agents pa ON pa.agent_id=a.id "
            "JOIN project_members pm ON pm.project_id=pa.project_id AND pm.user_id=$2 "
            "WHERE a.id=$1 ORDER BY CASE pm.role WHEN 'owner' THEN 0 WHEN 'editor' THEN 1 ELSE 2 END LIMIT 1",
            {resource_id, actor_user_id});
    } else if (resource_type == "CONVERSATION") {
        r = s.query(
            "SELECT pm.role FROM conversations c JOIN project_members pm ON pm.project_id=c.project_id "
            "WHERE c.id=$1 AND pm.user_id=$2",
            {resource_id, actor_user_id});
    } else if (resource_type == "RUN") {
        r = s.query(
            "SELECT pm.role FROM runs r JOIN conversations c ON c.id=r.conversation_id "
            "JOIN project_members pm ON pm.project_id=c.project_id "
            "WHERE r.id=$1 AND pm.user_id=$2",
            {resource_id, actor_user_id});
    } else {
        return {false, "unsupported resource type"};
    }
    return r.rows() == 1 ? std::pair{true, r.get(0, 0)} : std::pair{false, std::string("not a project member")};
}

} // namespace ws
