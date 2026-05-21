FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt update && apt install -y \
    build-essential \
    cmake \
    ninja-build \
    clang \
    lld \
    git \
    pkg-config \
    dpkg-dev \
    fakeroot

WORKDIR /build

COPY . .

RUN cmake -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release

RUN cmake --build build

RUN cpack -G DEB --config build/CPackConfig.cmake
