FROM fedora:41

ENV CC=clang
ENV CXX=clang++

RUN dnf install -y \
    clang \
    cmake \
    ninja-build \
    git \
    make \
    gcc-c++ \
    rpm-build \
    rpmdevtools \
    file \
    fmt-devel \
 && dnf clean all \
 && rm -rf /var/cache/dnf

WORKDIR /build

COPY . .

RUN cmake -B build \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DMETIS_USE_SYSTEM_FMT=ON \
    -DMETIS_USE_SYSTEM_TOMLPLUSPLUS=OFF

RUN cmake --build build --parallel

RUN cpack -G RPM --config build/CPackConfig.cmake

RUN mkdir -p /artifacts && \
    cp /build/*.rpm /artifacts/

CMD ["bash"]
