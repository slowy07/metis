FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive
ENV CC=clang
ENV CXX=clang++

RUN apt update && apt install -y \
    build-essential \
    cmake \
    ninja-build \
    clang \
    lld \
    git \
    pkg-config \
    dpkg-dev \
    fakeroot \
    libfmt-dev \
    tomlplusplus-dev

WORKDIR /build

COPY . .

RUN cmake -B build \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DSNIFFERCOMMIT_USE_SYSTEM_DEPS=ON

RUN cmake --build build --parallel

RUN cpack -G DEB --config build/CPackConfig.cmake
