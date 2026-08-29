#!/usr/bin/env bash

set -euo pipefail

targets=(
  archlinux
  ubuntu
  fedora
  alpine
)

for target in "${targets[@]}"; do
  echo "BUILDING: $target"

  podman build \
    -f docker/$target/Dockerfile \
    -t metis-$target \
    .

  echo
done
