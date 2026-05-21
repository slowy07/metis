#!/usr/bin/env bash

set -euo pipefail

mkdir -p artifacts

extract() {
  local image="$1"
  local src="$2"

  echo "extracting from $image"

  local cid
  cid=$(podman create "$image")

  podman cp "$cid:$src" artifacts/

  podman rm "$cid"
  echo
}

extract sniffercommit-ubuntu \
  /artifacts

extract sniffercommit-fedora \
  /artifacts

extract sniffercommit-archlinux \
  /home/builder/artifacts

extract sniffercommit-alpine \
  /artifacts
