#include "core.hpp"
#include "grpc_server.hpp"
#include "http_server.hpp"
#include <drogon/drogon.h>
#include <thread>

int main()
{
    using namespace ws;
    try {
        Config config = Config::load();
        Database database(config.database_url);
        database.applyMigrations(config.migrations_dir);
        CoreService service(config);
        std::thread grpc_thread([&service, &config]() { runGrpcServer(service, config); });
        grpc_thread.detach();
        runHttpServer(service, config);
    } catch (const std::exception& e) {
        LOG_ERROR << "fatal startup error: " << e.what();
        return 1;
    }
    return 0;
}