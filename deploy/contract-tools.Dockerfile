FROM ubuntu:24.04

ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        bash \
        ca-certificates \
        protobuf-compiler \
        python3 \
        python3-yaml \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /work
