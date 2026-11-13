#include "core.hpp"
#include "grpc_server.hpp"
#include "http_server.hpp"
#include <drogon/drogon.h>
#include <grpcpp/grpcpp.h>
#include <exception>
#include <future>
#include <memory>
#include <stdexcept>
#include <thread>

int main()
{
    using namespace ws;
    std::string stage = "config.load";
    std::thread grpc_thread;
    std::shared_ptr<grpc::Server> grpc_server;
    auto stop_grpc = [&]() {
        if (grpc_server) grpc_server->Shutdown();
        if (grpc_thread.joinable()) grpc_thread.join();
    };
    try {
        LOG_INFO << "startup stage=config.load";
        Config config = Config::load();

        stage = "database.connect";
        LOG_INFO << "startup stage=database.connect";
        Database database(config.database_url);
        (void)database.session();

        stage = "database.migrate";
        LOG_INFO << "startup stage=database.migrate directory=" << config.migrations_dir;
        database.applyMigrations(config.migrations_dir);

        CoreService service(config);
        stage = "grpc.start";
        LOG_INFO << "startup stage=grpc.start";
        std::promise<std::shared_ptr<grpc::Server>> grpc_started;
        grpc_thread = std::thread([&]() {
            bool announced = false;
            try {
                runGrpcServer(service, config, [&](std::shared_ptr<grpc::Server> server) {
                    announced = true;
                    grpc_started.set_value(std::move(server));
                });
            } catch (const std::exception& e) {
                if (!announced) grpc_started.set_exception(std::current_exception());
                else LOG_ERROR << "gRPC server stopped with error: " << e.what();
            }
        });
        grpc_server = grpc_started.get_future().get();
        if (!grpc_server) throw std::runtime_error("gRPC server failed to bind");

        stage = "http.start";
        LOG_INFO << "startup stage=http.start";
        runHttpServer(service, config);

        stage = "shutdown";
        LOG_INFO << "startup stage=shutdown";
        stop_grpc();
    } catch (const std::exception& e) {
        LOG_ERROR << "fatal startup error stage=" << stage << " error=" << e.what();
        stop_grpc();
        return 1;
    } catch (...) {
        LOG_ERROR << "fatal startup error stage=" << stage << " error=unknown";
        stop_grpc();
        return 1;
    }
    return 0;
}
