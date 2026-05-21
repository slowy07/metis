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
      sudo

RUN useradd -m builder && \
    echo "builder ALL=(ALL) NOPASSWRD: ALL" >> /etc/sudoers

USER builder
WORKDIR /home/builder/build

COPY --chown=builder:builder . .

RUN makepkg -sf --noconfirm
