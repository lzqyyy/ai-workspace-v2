#pragma once
#include "core.hpp"
#include <functional>
#include <memory>
namespace grpc { class Server; }
namespace ws { void runGrpcServer(CoreService& service, const Config& config, std::function<void(std::shared_ptr<grpc::Server>)> on_started); }
