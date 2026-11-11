#include "http_server.hpp"
#include <drogon/drogon.h>
#include <atomic>
#include <memory>
using namespace drogon;
namespace ws {
namespace {
std::string required_request_id(const HttpRequestPtr& req) {
    auto value = req->getHeader("x-request-id");
    if (value.empty()) value = req->getParameter("request_id");
    if (trim(value).empty()) throw ServiceError(400, "VALIDATION_FAILED", "X-Request-ID is required");
    return request_id_from(value);
}
Json parse_body(const HttpRequestPtr& req) {
    if (req->body().empty()) throw ServiceError(400, "VALIDATION_FAILED", "JSON body is required");
    Json body;
    try { body = Json::parse(std::string(req->body())); }
    catch (...) { throw ServiceError(400, "VALIDATION_FAILED", "request body is invalid JSON"); }
    if (!body.is_object()) throw ServiceError(400, "VALIDATION_FAILED", "request body must be an object");
    return body;
}
HttpResponsePtr json_response(const Json& body, int status, const std::string& rid) {
    auto response = HttpResponse::newHttpResponse();
    response->setStatusCode(static_cast<HttpStatusCode>(status));
    response->setContentTypeCode(CT_APPLICATION_JSON);
    response->addHeader("Cache-Control", "no-store");
    response->addHeader("X-Request-ID", rid);
    response->setBody(body.dump());
    return response;
}
HttpResponsePtr error_response(const ServiceError& error, const std::string& rid) {
    return json_response({{"code", error.code}, {"message", error.what()}, {"request_id", rid}, {"retryable", error.retryable}}, error.http_status, rid);
}
HttpResponsePtr internal_error(const std::string& rid) {
    return error_response(ServiceError(500, "INTERNAL", "service temporarily unavailable", true), rid);
}
class Api {
public:
    Api(CoreService& service, const Config& config) : service_(service), config_(config) {}
    void registerRoutes();
private:
    AuthSession authenticate(const HttpRequestPtr& req) { return service_.authenticate(req->getCookie("workspace_session")); }
    void enforce_origin(const HttpRequestPtr& req) {
        const auto& origin = req->getHeader("origin");
        if (origin.empty()) return;
        for (const auto& candidate : split(config_.app_origin, ',')) {
            if (trim(candidate) == origin) return;
        }
        throw ServiceError(403, "FORBIDDEN", "request origin is not allowed");
    }
    template <typename F>
    void run_json(const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>& cb, bool authenticated, F&& operation, int status = 200) {
        const std::string rid = request_id_from(req->getHeader("x-request-id"));
        try {
            const std::string checked = required_request_id(req);
            std::optional<AuthSession> session;
            if (authenticated) session = authenticate(req);
            cb(json_response(operation(session.value_or(AuthSession{}), checked), status, checked));
        } catch (const ServiceError& e) { cb(error_response(e, rid)); }
        catch (...) { cb(internal_error(rid)); }
    }
    void registerRunRoutes();
    CoreService& service_;
    const Config& config_;
};
} // namespace

void Api::registerRoutes()
{
    auto& app = drogon::app();
    app.registerHandler("/api/v1/health", [this](const HttpRequestPtr&, std::function<void(const HttpResponsePtr&)>&& cb) {
        Json result;
        try { result = service_.health(); }
        catch (...) { result = {{"status", "degraded"}, {"database", "unavailable"}, {"version", "1.0.0"}}; }
        cb(json_response(result, 200, "req-health"));
    }, {Get});

    app.registerHandler("/api/v1/auth/login", [this](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& cb) {
        const std::string rid = request_id_from(req->getHeader("x-request-id"));
        try {
            enforce_origin(req);
            auto body = parse_body(req);
            auto result = service_.login(body.value("username", ""), body.value("password", ""), required_request_id(req));
            const std::string token = result.value("token", "");
            result.erase("token");
            auto response = json_response(result, 200, rid);
            Cookie cookie("workspace_session", token);
            cookie.setPath("/");
            cookie.setHttpOnly(true);
            cookie.setSecure(config_.cookie_secure);
            cookie.setSameSite(Cookie::SameSite::kLax);
            cookie.setMaxAge(config_.session_ttl_seconds);
            response->addCookie(std::move(cookie));
            cb(response);
        } catch (const ServiceError& e) { cb(error_response(e, rid)); }
        catch (...) { cb(internal_error(rid)); }
    }, {Post});

    app.registerHandler("/api/v1/auth/logout", [this](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& cb) {
        const std::string rid = request_id_from(req->getHeader("x-request-id"));
        try {
            enforce_origin(req);
            auto session = authenticate(req);
            service_.logout(session.token, session.user_id, required_request_id(req));
            auto response = HttpResponse::newHttpResponse();
            response->setStatusCode(k204NoContent);
            Cookie cookie("workspace_session", "");
            cookie.setPath("/");
            cookie.setMaxAge(0);
            cookie.setHttpOnly(true);
            response->addCookie(std::move(cookie));
            cb(response);
        } catch (const ServiceError& e) { cb(error_response(e, rid)); }
        catch (...) { cb(internal_error(rid)); }
    }, {Post});

    app.registerHandler("/api/v1/me", [this](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& cb) {
        run_json(req, cb, true, [this](const AuthSession& s, const std::string&) { return service_.currentUser(s.user_id); });
    }, {Get});

    app.registerHandler("/api/v1/agents", [this](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& cb) {
        if (req->method() == Get) {
            run_json(req, cb, true, [this](const AuthSession& s, const std::string& rid) { return service_.listAgents(s.user_id, rid); });
        } else {
            run_json(req, cb, true, [this, req](const AuthSession& s, const std::string& rid) {
                enforce_origin(req); return service_.createAgent(s.user_id, parse_body(req), rid);
            }, 201);
        }
    }, {Get, Post});

    app.registerHandler("/api/v1/conversations", [this](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& cb) {
        if (req->method() == Get) {
            run_json(req, cb, true, [this](const AuthSession& s, const std::string& rid) { return service_.listConversations(s.user_id, rid); });
        } else {
            run_json(req, cb, true, [this, req](const AuthSession& s, const std::string& rid) {
                enforce_origin(req); return service_.createConversation(s.user_id, parse_body(req), rid);
            }, 201);
        }
    }, {Get, Post});

    app.registerHandler("/api/v1/conversations/{1}/messages", [this](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& cb, const std::string& conversationId) {
        const std::string conversation_id = conversationId;
        if (req->method() == Get) {
            run_json(req, cb, true, [this, req, conversation_id](const AuthSession& s, const std::string& rid) {
                long long after = 0;
                try { after = std::stoll(req->getParameter("after_seq")); } catch (...) {}
                return service_.listMessages(s.user_id, conversation_id, after, rid);
            });
        } else {
            run_json(req, cb, true, [this, req, conversation_id](const AuthSession& s, const std::string& rid) {
                enforce_origin(req);
                std::string client_id = req->getHeader("idempotency-key");
                auto body = parse_body(req);
                if (client_id.empty()) client_id = body.value("client_request_id", "");
                return service_.sendMessage(s.user_id, conversation_id, body, client_id, rid);
            }, 202);
        }
    }, {Get, Post});

    registerRunRoutes();
}

struct StreamState {
    std::shared_ptr<ResponseStream> stream;
    std::function<void()> pump;
    trantor::TimerId timer{0};
    long long cursor{0};
    std::chrono::steady_clock::time_point last_heartbeat{std::chrono::steady_clock::now()};
    bool closed{false};
};

void Api::registerRunRoutes()
{
    auto& app = drogon::app();
    app.registerHandler("/api/v1/runs/{1}/cancel", [this](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& cb, const std::string& runId) {
        const std::string run_id = runId;
        run_json(req, cb, true, [this, req, run_id](const AuthSession& s, const std::string& rid) {
            enforce_origin(req); return service_.cancelRun(s.user_id, run_id, rid);
        });
    }, {Post});

    app.registerHandler("/api/v1/runs/{1}/retry", [this](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& cb, const std::string& runId) {
        const std::string run_id = runId;
        run_json(req, cb, true, [this, req, run_id](const AuthSession& s, const std::string& rid) {
            enforce_origin(req); return service_.retryRun(s.user_id, run_id, rid);
        }, 202);
    }, {Post});

    app.registerHandler("/api/v1/runs/{1}/events", [this](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& cb, const std::string& eventRunId) {
        const std::string rid = request_id_from(req->getParameter("request_id"));
        try {
            const std::string checked = required_request_id(req);
            auto session = authenticate(req);
            long long cursor = 0;
            std::string last = req->getHeader("last-event-id");
            if (last.empty()) last = req->getParameter("after");
            try { cursor = last.empty() ? 0 : std::stoll(last); } catch (...) { cursor = 0; }
            (void)service_.listRunEvents(session.user_id, eventRunId, cursor);
            const std::string run_id = eventRunId;
            const std::string user_id = session.user_id;
            auto state = std::make_shared<StreamState>();
            state->cursor = cursor;
            auto response = HttpResponse::newAsyncStreamResponse(
                [this, state, user_id, run_id, checked](ResponseStreamPtr stream) {
                    state->stream = std::shared_ptr<ResponseStream>(stream.release());
                    state->stream->send("retry: 2000\n\n");
                    auto close = [state]() {
                        if (state->closed) return;
                        state->closed = true;
                        if (state->timer) drogon::app().getLoop()->invalidateTimer(state->timer);
                        state->stream->close();
                    };
                    state->pump = [this, state, user_id, run_id, checked, close]() {
                        if (state->closed) return;
                        try {
                            auto result = service_.listRunEvents(user_id, run_id, state->cursor);
                            bool terminal = false;
                            for (const auto& event : result["items"]) {
                                state->cursor = event["seq"].get<long long>();
                                const std::string type = event["type"].get<std::string>();
                                const std::string frame = "id: " + std::to_string(state->cursor) + "\nevent: " + type +
                                                          "\ndata: " + event.dump() + "\n\n";
                                if (!state->stream->send(frame)) { close(); return; }
                                if (type == "message.completed" || type == "run.failed" || type == "run.cancelled") terminal = true;
                            }
                            if (terminal) { close(); return; }
                            auto now = std::chrono::steady_clock::now();
                            if (now - state->last_heartbeat >= std::chrono::seconds(15)) {
                                state->last_heartbeat = now;
                                if (!state->stream->send(": heartbeat " + std::to_string(std::time(nullptr)) + "\n\n")) close();
                            }
                        } catch (...) { close(); }
                    };
                    state->timer = drogon::app().getLoop()->runEvery(0.5, [state]() { if (state->pump) state->pump(); });
                    state->pump();
                }, true);
            response->setStatusCode(k200OK);
            response->setContentTypeString("text/event-stream; charset=utf-8");
            response->addHeader("Cache-Control", "no-cache, no-transform");
            response->addHeader("X-Accel-Buffering", "no");
            response->addHeader("X-Request-ID", checked);
            cb(response);
        } catch (const ServiceError& e) { cb(error_response(e, rid)); }
        catch (...) { cb(internal_error(rid)); }
    }, {Get});
}
void runHttpServer(CoreService& service, const Config& config)
{
    auto& app = drogon::app();
    app.setThreadNum(4);
    app.setDocumentRoot(config.frontend_dir);
    app.addListener(config.http_host, config.http_port);
    app.registerPreSendingAdvice([](const HttpRequestPtr&, const HttpResponsePtr& response) {
        response->addHeader("X-Content-Type-Options", "nosniff");
        response->addHeader("Referrer-Policy", "no-referrer");
        response->addHeader("Permissions-Policy", "camera=(), microphone=(), geolocation=()");
        response->addHeader("Content-Security-Policy",
                            "default-src 'self'; connect-src 'self'; img-src 'self' data:; "
                            "script-src 'self'; style-src 'self'; base-uri 'none'; frame-ancestors 'none'");
    });
    Api api(service, config);
    api.registerRoutes();
    LOG_INFO << "HTTP/SSE listening on " << config.http_host << ':' << config.http_port;
    app.run();
}

} // namespace ws