#!/usr/bin/env bash

set -euo pipefail

mkdir -p artifacts

extract() {
  local image="$1"
  local src="$2"

  local cid
  cid=$(podman create "$image")

  podman cp "$cid:$src" artifacts/

  podman rm "$cid"
}

extract sniffercommit-ubuntu \
  /build/*.deb

extract sniffercommit-fedora \
  /build/*.rpm

extract sniffercommit-archlinux \
  /home/builder/project/packaging/aur/*.pkg.tar.zst
