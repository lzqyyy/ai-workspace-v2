FROM drogonframework/drogon:v1.9.14 AS drogon

FROM ubuntu:24.04 AS build
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake ninja-build pkg-config \
    libgrpc++-dev libprotobuf-dev protobuf-compiler protobuf-compiler-grpc \
    libpq-dev libssl-dev libc-ares-dev libjsoncpp-dev uuid-dev zlib1g-dev \
    libsqlite3-dev libhiredis-dev libmariadb-dev nlohmann-json3-dev \
    && rm -rf /var/lib/apt/lists/*
COPY --from=drogon /usr/local/include/drogon /usr/local/include/drogon
COPY --from=drogon /usr/local/include/trantor /usr/local/include/trantor
COPY --from=drogon /usr/local/lib/libdrogon.a /usr/local/lib/libdrogon.a
COPY --from=drogon /usr/local/lib/libtrantor.a /usr/local/lib/libtrantor.a
COPY --from=drogon /usr/local/lib/cmake /usr/local/lib/cmake
WORKDIR /src
COPY CMakeLists.txt ./
COPY proto ./proto
COPY server ./server
RUN cmake -S . -B /build -G Ninja -DCMAKE_BUILD_TYPE=Release \
    && cmake --build /build -j2

FROM ubuntu:24.04
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    libgrpc++1.51t64 libprotobuf32t64 libc-ares2 libpq5 libssl3 \
    libjsoncpp25 libuuid1 zlib1g libmariadb3 libhiredis1.1.0 libsqlite3-0 \
    curl ca-certificates \
    && rm -rf /var/lib/apt/lists/* \
    && useradd --system --uid 10001 --create-home appuser
WORKDIR /app
COPY --from=build /build/workspace_server /app/workspace_server
COPY frontend /app/frontend
COPY db/migrations /app/migrations
ENV HTTP_HOST=0.0.0.0 HTTP_PORT=8080 GRPC_HOST=0.0.0.0 GRPC_PORT=50051 \
    FRONTEND_DIR=/app/frontend MIGRATIONS_DIR=/app/migrations
USER appuser
EXPOSE 8080
ENTRYPOINT ["/app/workspace_server"]