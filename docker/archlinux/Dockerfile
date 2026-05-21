FROM archlinux:latest

RUN pacman -Syu --noconfirm && \
    pacman -S --noconfirm \
        base-devel \
        cmake \
        ninja \
        git \
        clang \
        lld \
        fmt \
        tomlplusplus \
        sudo

RUN useradd -m builder && \
    echo "builder ALL=(ALL) NOPASSWD: ALL" >> /etc/sudoers

USER builder

WORKDIR /home/builder/project

COPY --chown=builder:builder . .

WORKDIR /home/builder/project/packaging/aur

RUN makepkg -sf --noconfirm
