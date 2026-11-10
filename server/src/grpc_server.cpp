#include "grpc_server.hpp"
#include "workspace.grpc.pb.h"
#include <grpcpp/grpcpp.h>
#include <drogon/drogon.h>
namespace ws {
namespace {
using aiworkspace::v1::JobStatus;
using aiworkspace::v1::RunStatus;
std::string rid(const aiworkspace::v1::RequestMeta& meta) { return request_id_from(meta.request_id()); }
grpc::Status to_status(const ServiceError& e) {
    grpc::StatusCode code = grpc::StatusCode::INTERNAL;
    if (e.code == "VALIDATION_FAILED") code = grpc::StatusCode::INVALID_ARGUMENT;
    else if (e.code == "AUTH_REQUIRED") code = grpc::StatusCode::UNAUTHENTICATED;
    else if (e.code == "FORBIDDEN") code = grpc::StatusCode::PERMISSION_DENIED;
    else if (e.code == "NOT_FOUND") code = grpc::StatusCode::NOT_FOUND;
    else if (e.code == "JOB_LEASE_INVALID" || e.code == "RUN_CANCELLED" || e.code == "CONFLICT") code = grpc::StatusCode::FAILED_PRECONDITION;
    return {code, e.code + ": " + e.what()};
}
JobStatus job_status(const std::string& value) {
    if (value == "queued") return aiworkspace::v1::JOB_STATUS_QUEUED;
    if (value == "running") return aiworkspace::v1::JOB_STATUS_RUNNING;
    if (value == "succeeded") return aiworkspace::v1::JOB_STATUS_SUCCEEDED;
    if (value == "failed") return aiworkspace::v1::JOB_STATUS_FAILED;
    if (value == "cancelled") return aiworkspace::v1::JOB_STATUS_CANCELLED;
    return aiworkspace::v1::JOB_STATUS_UNSPECIFIED;
}
RunStatus run_status(const std::string& value) {
    if (value == "queued") return aiworkspace::v1::RUN_STATUS_QUEUED;
    if (value == "running") return aiworkspace::v1::RUN_STATUS_RUNNING;
    if (value == "succeeded") return aiworkspace::v1::RUN_STATUS_SUCCEEDED;
    if (value == "failed") return aiworkspace::v1::RUN_STATUS_FAILED;
    if (value == "cancelled") return aiworkspace::v1::RUN_STATUS_CANCELLED;
    return aiworkspace::v1::RUN_STATUS_UNSPECIFIED;
}
std::string job_status_name(JobStatus status) {
    switch (status) {
    case aiworkspace::v1::JOB_STATUS_SUCCEEDED: return "succeeded";
    case aiworkspace::v1::JOB_STATUS_FAILED: return "failed";
    case aiworkspace::v1::JOB_STATUS_CANCELLED: return "cancelled";
    case aiworkspace::v1::JOB_STATUS_RUNNING: return "running";
    default: return "queued";
    }
}
class JobServiceImpl final : public aiworkspace::v1::JobService::Service {
public:
    explicit JobServiceImpl(CoreService& service) : service_(service) {}
    grpc::Status ClaimNextJob(grpc::ServerContext*, const aiworkspace::v1::ClaimNextJobRequest* request, aiworkspace::v1::ClaimNextJobResponse* response) override {
        try {
            auto result = service_.claimNextJob(request->worker_id(), rid(request->meta()));
            response->set_job_available(result.value("has_job", false));
            response->set_poll_after_ms(result.value("poll_after_ms", 750));
            if (result.value("has_job", false)) {
                const auto& value = result["job"];
                auto* lease = response->mutable_job();
                lease->set_job_id(value.value("job_id", "")); lease->set_run_id(value.value("run_id", ""));
                lease->set_worker_id(value.value("worker_id", "")); lease->set_lease_token(value.value("lease_token", ""));
                lease->set_lease_expires_at_unix_ms(value.value("lease_expires_at_unix_ms", 0LL));
                lease->set_attempt(value.value("attempt", 0)); lease->set_max_attempts(value.value("max_attempts", 3));
                lease->set_status(job_status(value.value("status", "running")));
            }
            return grpc::Status::OK;
        } catch (const ServiceError& e) { return to_status(e); }
        catch (...) { return {grpc::StatusCode::INTERNAL, "INTERNAL: unexpected error"}; }
    }    grpc::Status Heartbeat(grpc::ServerContext*, const aiworkspace::v1::HeartbeatRequest* request, aiworkspace::v1::HeartbeatResponse* response) override {
        try {
            auto result = service_.heartbeat(request->job_id(), request->run_id(), request->lease_token(), request->lease_seconds(), rid(request->meta()));
            response->set_lease_valid(result.value("lease_valid", false));
            response->set_cancel_requested(result.value("cancel_requested", false));
            response->set_job_status(job_status(result.value("job_status", "failed")));
            response->set_lease_expires_at_unix_ms(result.value("lease_expires_at_unix_ms", 0LL));
            return grpc::Status::OK;
        } catch (const ServiceError& e) { return to_status(e); }
        catch (...) { return {grpc::StatusCode::INTERNAL, "INTERNAL: unexpected error"}; }
    }
    grpc::Status UpdateJobStatus(grpc::ServerContext*, const aiworkspace::v1::UpdateJobStatusRequest* request, aiworkspace::v1::UpdateJobStatusResponse* response) override {
        try {
            auto result = service_.updateJobStatus(request->job_id(), request->run_id(), request->lease_token(),
                job_status_name(request->status()), request->error_code(), request->error_message(),
                request->final_content(), request->usage_json(), rid(request->meta()));
            response->set_accepted(result.value("accepted", false));
            response->set_duplicate(result.value("duplicate", false));
            response->set_run_status(run_status(result.value("run_status", "failed")));
            response->set_final_event_seq(result.value("final_event_seq", 0LL));
            return grpc::Status::OK;
        } catch (const ServiceError& e) { return to_status(e); }
        catch (...) { return {grpc::StatusCode::INTERNAL, "INTERNAL: unexpected error"}; }
    }
private:
    CoreService& service_;
};

class ContextServiceImpl final : public aiworkspace::v1::ContextService::Service {
public:
    explicit ContextServiceImpl(CoreService& service) : service_(service) {}
    grpc::Status GetExecutionContext(grpc::ServerContext*, const aiworkspace::v1::GetExecutionContextRequest* request, aiworkspace::v1::GetExecutionContextResponse* response) override {
        try {
            auto result = service_.getExecutionContext(request->job_id(), request->run_id(), request->lease_token(), rid(request->meta()));
            response->set_run_id(result.value("run_id", "")); response->set_job_id(result.value("job_id", ""));
            response->set_agent_id(result.value("agent_id", "")); response->set_conversation_id(result.value("conversation_id", ""));
            response->set_system_prompt(result.value("system_prompt", "")); response->set_provider_type(result.value("provider_type", ""));
            response->set_model_name(result.value("model_name", "")); response->set_base_url(result.value("base_url", ""));
            response->set_secret_ref(result.value("secret_ref", "")); response->set_next_event_seq(result.value("next_event_seq", 1LL));
            response->set_lease_expires_at_unix_ms(result.value("lease_expires_at_unix_ms", 0LL));
            for (const auto& item : result["messages"]) {
                auto* message = response->add_messages();
                message->set_message_id(item.value("message_id", "")); message->set_role(item.value("role", ""));
                message->set_sender_type(item.value("sender_type", "")); message->set_content(item.value("content", ""));
                message->set_seq(item.value("seq", 0LL));
            }
            return grpc::Status::OK;
        } catch (const ServiceError& e) { return to_status(e); }
        catch (...) { return {grpc::StatusCode::INTERNAL, "INTERNAL: unexpected error"}; }
    }
private:
    CoreService& service_;
};class RunEventServiceImpl final : public aiworkspace::v1::RunEventService::Service {
public:
    explicit RunEventServiceImpl(CoreService& service) : service_(service) {}
    grpc::Status AppendEvent(grpc::ServerContext*, const aiworkspace::v1::AppendEventRequest* request, aiworkspace::v1::AppendEventResponse* response) override {
        try {
            auto result = service_.appendEvent(request->job_id(), request->run_id(), request->lease_token(),
                request->event_seq(), request->event_type(), request->payload_json(), rid(request->meta()));
            response->set_accepted(result.value("accepted", false));
            response->set_duplicate(result.value("duplicate", false));
            response->set_stored_seq(result.value("stored_seq", 0LL));
            response->set_next_seq(result.value("next_seq", 0LL));
            return grpc::Status::OK;
        } catch (const ServiceError& e) { return to_status(e); }
        catch (...) { return {grpc::StatusCode::INTERNAL, "INTERNAL: unexpected error"}; }
    }
private:
    CoreService& service_;
};

class PermissionServiceImpl final : public aiworkspace::v1::PermissionService::Service {
public:
    explicit PermissionServiceImpl(CoreService& service) : service_(service) {}
    grpc::Status CheckPermission(grpc::ServerContext*, const aiworkspace::v1::CheckPermissionRequest* request, aiworkspace::v1::CheckPermissionResponse* response) override {
        try {
            std::string type;
            switch (request->resource_type()) {
            case aiworkspace::v1::RESOURCE_TYPE_AGENT: type = "AGENT"; break;
            case aiworkspace::v1::RESOURCE_TYPE_CONVERSATION: type = "CONVERSATION"; break;
            case aiworkspace::v1::RESOURCE_TYPE_RUN: type = "RUN"; break;
            default: return {grpc::StatusCode::INVALID_ARGUMENT, "VALIDATION_FAILED: unsupported resource type"};
            }
            auto [allowed, reason] = service_.checkPermission(request->actor_user_id(), type, request->resource_id(), request->action());
            response->set_allowed(allowed); response->set_reason(reason);
            return grpc::Status::OK;
        } catch (const ServiceError& e) { return to_status(e); }
        catch (...) { return {grpc::StatusCode::INTERNAL, "INTERNAL: unexpected error"}; }
    }
private:
    CoreService& service_;
};

} // namespace

void runGrpcServer(CoreService& service, const Config& config)
{
    JobServiceImpl job(service);
    ContextServiceImpl context(service);
    RunEventServiceImpl events(service);
    PermissionServiceImpl permission(service);
    std::string address = config.grpc_host + ":" + std::to_string(config.grpc_port);
    grpc::ServerBuilder builder;
    builder.AddListeningPort(address, grpc::InsecureServerCredentials());
    builder.RegisterService(&job);
    builder.RegisterService(&context);
    builder.RegisterService(&events);
    builder.RegisterService(&permission);
    auto server = builder.BuildAndStart();
    if (!server) {
        LOG_ERROR << "gRPC server failed to bind " << address;
        return;
    }
    LOG_INFO << "gRPC listening on " << address;
    server->Wait();
}

} // namespace ws