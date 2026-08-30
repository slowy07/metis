FROM archlinux:latest

ENV CC=clang
ENV CXX=clang++

RUN pacman -Syu --noconfirm && \
    pacman -S --noconfirm --needed \
        base-devel \
        cmake \
        ninja \
        git \
        clang \
        lld \
        fmt \
        sudo

RUN useradd -m builder && \
    echo "builder ALL=(ALL) NOPASSWD: ALL" >> /etc/sudoers

USER builder

WORKDIR /home/builder/project

COPY --chown=builder:builder . .

RUN cmake -B build -DCMAKE_INSTALL_PREFIX=/usr && \
    make -C build

RUN mkdir -p /home/builder/artifacts && \
    cp build/metis /home/builder/artifacts/metis

CMD ["bash"]
