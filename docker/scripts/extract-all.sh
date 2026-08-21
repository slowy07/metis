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

extract metis-ubuntu /artifacts
extract metis-fedora /artifacts
extract metis-archlinux /home/builder/artifacts
extract metis-alpine /artifacts
