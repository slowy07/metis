FROM fedora:42

RUN dnf install -y \
    gcc-c++ \
    clang \
    lld \
    cmake \
    ninja-build \
    git \
    rpm-build \
    rpmdevtools \
    make

WORKDIR /build

COPY . .

RUN cmake -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release

RUN cmake --build build

RUN cpack -G RPM --config build/CPackConfig.cmake
