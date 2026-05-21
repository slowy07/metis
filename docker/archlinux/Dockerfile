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

RUN useradd -m builder

USER builder
WORKDIR /home/builder/build

COPY --chown=builder:builder . .

RUN makepkg -sf --noconfirm
