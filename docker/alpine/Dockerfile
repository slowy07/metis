FROM alpine:3.21

ENV CC=clang
ENV CXX=clang++

RUN apk add --no-cache \
    build-base \
    cmake \
    ninja \
    clang \
    lld \
    git \
    fmt-dev

WORKDIR /build

COPY . .

RUN cmake -B build \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DMETIS_USE_SYSTEM_FMT=ON \
    -DMETIS_USE_SYSTEM_TOMLPLUSPLUS=OFF

RUN cmake --build build --parallel

RUN mkdir -p /artifacts && \
    cp build/bin/metis /artifacts/

CMD ["sh"]
