FROM fedora:42

ENV CC=clang
ENV CXX=clang++

RUN dnf install -y \
    cmake \
    ninja-build \
    clang \
    lld \
    gcc-c++ \
    git \
    rpm-build \
    fmt-devel

WORKDIR /build

COPY . .

RUN cmake -B build \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DSNIFFERCOMMIT_USE_SYSTEM_FMT=ON \
    -DSNIFFERCOMMIT_USE_SYSTEM_TOMLPLUSPLUS=OFF

RUN cmake --build build --parallel

RUN cpack -G RPM --config build/CPackConfig.cmake
