FROM ubuntu:24.04 AS build
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake ninja-build pkg-config \
    libdrogon-dev libgrpc++-dev libprotobuf-dev protobuf-compiler protobuf-compiler-grpc \
    libpq-dev libssl-dev libc-ares-dev nlohmann-json3-dev libmariadb-dev \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY CMakeLists.txt ./
COPY proto ./proto
COPY server ./server
RUN cmake -S . -B /build -G Ninja -DCMAKE_BUILD_TYPE=Release \
    && cmake --build /build -j2

FROM ubuntu:24.04
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    libdrogon-dev libgrpc++-dev libprotobuf-dev libpq5 libssl3 libc-ares2 nlohmann-json3-dev libmariadb-dev curl ca-certificates \
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