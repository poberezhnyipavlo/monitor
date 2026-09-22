FROM ubuntu:latest

LABEL authors="pavlopobereznij"

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    gdb \
    pkg-config \
    librrd-dev \
    && rm -rf /var/lib/apt/list/*

WORKDIR /app