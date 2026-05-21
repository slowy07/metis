#!/usr/bin/env bash

set -euo pipefail

mkdir -p artifacts

extract() {
  local image="$1"
  local pattern="$2"

  cid=$(podman create "$image")
  
  podman cp "$cid":"$pattern" artifacts/ || true

  podman rm "$cid"
}

extract sniffercommit-archlinux \
  /home/builder/build/*.pkg.tar.zst

extract sniffercommit-ubuntu \
  /build/*.deb

extract sniffercommit-fedora \
  /build/*.rpm

extract sniffercommit-alpine \
  /build/build/sniffercommit
