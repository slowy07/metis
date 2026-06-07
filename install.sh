#!/usr/bin/env bash
# sniffercommit install script
# Usage: curl -LsSf https://raw.githubusercontent.com/slowy07/sniffercommit/main/scripts/install.sh | sh
#        curl -LsSf https://raw.githubusercontent.com/slowy07/sniffercommit/main/scripts/install.sh | sh -s -- v0.3.3

set -eu

REPO="slowy07/sniffercommit"
INSTALL_DIR="${HOME}/.local/bin"
VERSION="${1:-latest}"

OS="$(uname -s)"
ARCH="$(uname -m)"

case "${OS}" in
  Linux)  PLATFORM="linux-x86_64" ;;
  Darwin) PLATFORM="macos-x86_64" ;;
  *)
    echo "Unsupported OS: ${OS}"
    exit 1
    ;;
esac

case "${ARCH}" in
  x86_64|amd64) ;;
  *)
    echo "Unsupported architecture: ${ARCH}"
    exit 1
    ;;
esac

if [ "${VERSION}" = "latest" ]; then
  BASE_URL="https://github.com/${REPO}/releases/latest/download"
else
  BASE_URL="https://github.com/${REPO}/releases/download/${VERSION}"
fi

DOWNLOAD_URL="${BASE_URL}/sniffercommit-${PLATFORM}.tar.gz"

echo "Downloading sniffercommit for ${PLATFORM}..."

TEMP_DIR="$(mktemp -d)"
trap 'rm -rf "${TEMP_DIR}"' EXIT

if command -v curl >/dev/null 2>&1; then
  curl -LsSf "${DOWNLOAD_URL}" -o "${TEMP_DIR}/sniffercommit.tar.gz"
elif command -v wget >/dev/null 2>&1; then
  wget -q "${DOWNLOAD_URL}" -O "${TEMP_DIR}/sniffercommit.tar.gz"
else
  echo "Error: need curl or wget"
  exit 1
fi

tar xzf "${TEMP_DIR}/sniffercommit.tar.gz" -C "${TEMP_DIR}"

mkdir -p "${INSTALL_DIR}"
cp "${TEMP_DIR}/sniffercommit" "${INSTALL_DIR}/sniffercommit"
chmod +x "${INSTALL_DIR}/sniffercommit"

echo "sniffercommit installed to ${INSTALL_DIR}/sniffercommit"

case ":${PATH}:" in
  *:"${INSTALL_DIR}":*) ;;
  *)
    echo ""
    echo "  ${INSTALL_DIR} is not in your PATH."
    echo "  Add this to your shell profile:"
    echo "    export PATH=\"\${HOME}/.local/bin:\${PATH}\""
    ;;
esac

echo "Run 'sniffercommit --help' to get started."
