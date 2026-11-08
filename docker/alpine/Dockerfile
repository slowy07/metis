FROM alpine:3.20

ENV CC=clang
ENV CXX=clang++

RUN apk add --no-cache \
    build-base \
    clang \
    lld \
    cmake \
    ninja \
    git \
    linux-headers \
    fmt-dev

WORKDIR /build

COPY . .

RUN cmake -B build \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DMETIS_USE_SYSTEM_FMT=ON \
    -DMETIS_USE_SYSTEM_TOMLPLUSPLUS=OFF

RUN cmake --build build --parallel

RUN cpack -G TGZ --config build/CPackConfig.cmake

RUN mkdir -p /artifacts && \
    cp /build/*.tar.gz /artifacts/

CMD ["bash"]
