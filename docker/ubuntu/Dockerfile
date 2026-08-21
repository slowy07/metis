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
    file \
    libfmt-dev \
 && apt clean \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /build

COPY . .

RUN cmake -B build \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DMETIS_USE_SYSTEM_FMT=ON \
    -DMETIS_USE_SYSTEM_TOMLPLUSPLUS=OFF

RUN cmake --build build --parallel

RUN cpack -G DEB --config build/CPackConfig.cmake

RUN mkdir -p /artifacts && \
    cp /build/*.deb /artifacts/

CMD ["bash"]
