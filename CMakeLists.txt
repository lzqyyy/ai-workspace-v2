cmake_minimum_required(VERSION 3.25)
project(ai_workspace_v2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)
find_package(Drogon CONFIG REQUIRED)
find_package(Protobuf REQUIRED)
find_package(c-ares CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(PostgreSQL REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

set(PROTO_SRC ${CMAKE_CURRENT_SOURCE_DIR}/proto/workspace.proto)
set(PROTO_OUT ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${PROTO_OUT})

add_custom_command(
    OUTPUT ${PROTO_OUT}/workspace.pb.cc ${PROTO_OUT}/workspace.pb.h
           ${PROTO_OUT}/workspace.grpc.pb.cc ${PROTO_OUT}/workspace.grpc.pb.h
    COMMAND protobuf::protoc
        --proto_path=${CMAKE_CURRENT_SOURCE_DIR}/proto
        --cpp_out=${PROTO_OUT}
        --grpc_out=${PROTO_OUT}
        --plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>
        ${PROTO_SRC}
    DEPENDS ${PROTO_SRC} protobuf::protoc gRPC::grpc_cpp_plugin
    VERBATIM)

add_executable(workspace_server
    server/src/main.cpp
    server/src/database.cpp
    server/src/core.cpp
    server/src/app.cpp
    server/src/grpc_server.cpp
    ${PROTO_OUT}/workspace.pb.cc
    ${PROTO_OUT}/workspace.grpc.pb.cc)

target_include_directories(workspace_server PRIVATE
    server/src
    ${PROTO_OUT}
    ${PostgreSQL_INCLUDE_DIRS})

target_link_libraries(workspace_server PRIVATE
    Drogon::Drogon
    gRPC::grpc++
    protobuf::libprotobuf
    PostgreSQL::PostgreSQL
    OpenSSL::Crypto
    nlohmann_json::nlohmann_json
    Threads::Threads)

if(WIN32)
    target_link_libraries(workspace_server PRIVATE ws2_32)
endif()

enable_testing()
add_test(NAME contract_validation COMMAND ${CMAKE_COMMAND} -E echo "Run scripts/test-contracts.ps1")