FROM archlinux:latest

ENV CC=clang
ENV CXX=clang++

RUN pacman -Syu --noconfirm \
    base-devel \
    clang \
    cmake \
    ninja \
    git \
    fmt \
 && pacman -Scc --noconfirm

WORKDIR /build

COPY . .

RUN cmake -B build \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DMETIS_USE_SYSTEM_FMT=ON \
    -DMETIS_USE_SYSTEM_TOMLPLUSPLUS=OFF

RUN cmake --build build --parallel

RUN cpack -G TGZ --config build/CPackConfig.cmake

RUN mkdir -p /home/builder/artifacts && \
    cp /build/*.tar.gz /home/builder/artifacts/

CMD ["bash"]
