#!/usr/bin/env bash

set -euo pipefail

targets=(
  archlinux
  ubuntu
  fedora
  alpine
)

for target in "${targets[@]}"; do
  echo "build $target"

  podman build \
    -f docker/$target/Dockerfile \
    -t sniffercommit-$target \
    .
done
