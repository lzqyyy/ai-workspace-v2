FROM golang:1.25-bookworm AS build
WORKDIR /src
COPY go.mod go.sum ./
RUN go mod download
COPY worker ./worker
RUN CGO_ENABLED=0 go build -buildvcs=false -trimpath -ldflags="-s -w" -o /out/workspace-worker ./worker/cmd/worker

FROM debian:bookworm-slim
RUN apt-get update && apt-get install -y --no-install-recommends ca-certificates \
    && rm -rf /var/lib/apt/lists/* \
    && useradd --system --uid 10002 --create-home worker
COPY --from=build /out/workspace-worker /usr/local/bin/workspace-worker
USER worker
ENTRYPOINT ["/usr/local/bin/workspace-worker"]