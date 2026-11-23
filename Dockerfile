FROM ubuntu:24.04

RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        ca-certificates \
        cmake \
        curl \
        gdb \
        git \
        libgrpc++-dev \
        liblmdb-dev \
        libprotobuf-dev \
        librrd-dev \
        libssl-dev \
        lmdb-utils \
        netcat-openbsd \
        openssl \
        pkg-config \
        protobuf-compiler \
        protobuf-compiler-grpc \
    && rm -rf /var/lib/apt/lists/*

ARG GRPCURL_VERSION=1.9.3
RUN arch="$(uname -m)" && [ "$arch" = "aarch64" ] && arch=arm64; \
    curl -fsSL "https://github.com/fullstorydev/grpcurl/releases/download/v${GRPCURL_VERSION}/grpcurl_${GRPCURL_VERSION}_linux_${arch}.tar.gz" \
        | tar -xz -C /usr/local/bin grpcurl

WORKDIR /app