#!/bin/sh
# sniffercommit install script
# Usage: curl -LsSf https://raw.githubusercontent.com/slowy07/sniffercommit/main/install.sh | sh
#        curl -LsSf https://raw.githubusercontent.com/slowy07/sniffercommit/main/install.sh | sh -s -- v0.3.9

set -eu

REPO="slowy07/sniffercommit"
INSTALL_DIR="${HOME}/.local/bin"
VERSION="${1:-latest}"

OS="$(uname -s)"
ARCH="$(uname -m)"

case "${OS}" in
    Linux) PLATFORM="linux-x86_64" ;;
    Darwin) PLATFORM="macos-x86_64" ;;
    *)
        echo "Unsupported OS: ${OS}"
        exit 1
        ;;
esac

case "${ARCH}" in
    x86_64 | amd64) ;;
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

DOWNLOAD_FILE="${TEMP_DIR}/sniffercommit.tar.gz"

if command -v curl >/dev/null 2>&1; then
    curl -LsSf "${DOWNLOAD_URL}" -o "${DOWNLOAD_FILE}" || download_failed=1
elif command -v wget >/dev/null 2>&1; then
    wget -q "${DOWNLOAD_URL}" -O "${DOWNLOAD_FILE}" || download_failed=1
else
    echo "Error: need curl or wget"
    exit 1
fi

if [ "${download_failed:-0}" -ne 0 ]; then
    echo "Warning: Failed to download sniffercommit from ${DOWNLOAD_URL}" >&2
    echo "Warning: No release found. Push a version tag or specify a version." >&2
    exit 0
fi

tar xzf "${DOWNLOAD_FILE}" -C "${TEMP_DIR}"

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
