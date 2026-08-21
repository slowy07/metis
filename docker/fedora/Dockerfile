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
    file \
    fmt-devel && \
    dnf clean all

WORKDIR /build

COPY . .

RUN cmake -B build \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DMETIS_USE_SYSTEM_FMT=ON \
    -DMETIS_USE_SYSTEM_TOMLPLUSPLUS=OFF

RUN cmake --build build --parallel

RUN mkdir -p /artifacts && \
    cpack \
      -G RPM \
      --config build/CPackConfig.cmake \
      -B /artifacts

CMD ["bash"]
