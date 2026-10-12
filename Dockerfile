FROM debian:bookworm-slim

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        bash \
        build-essential \
        ca-certificates \
        clang \
        clang-format \
        clinfo \
        cmake \
        coreutils \
        curl \
        gdb \
        git \
        libclang-dev \
        libgl-dev \
        libomp-dev \
        libx11-dev \
        libxcursor-dev \
        libxi-dev \
        libxinerama-dev \
        libxrandr-dev \
        llvm-dev \
        make \
        ninja-build \
        pocl-opencl-icd \
        python3 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace
