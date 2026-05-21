FROM alpine:3.22

RUN apk add --no-cache \
    build-base \
    clang \
    lld \
    cmake \
    ninja \
    git 

WORKDIR /build

COPY . .

RUN cmake -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release
    
RUN cmake --build build
