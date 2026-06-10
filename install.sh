#!/bin/sh
# sniffercommit install script
# Usage: curl -LsSf https://raw.githubusercontent.com/slowy07/sniffercommit/main/install.sh | sh
#        curl -LsSf https://raw.githubusercontent.com/slowy07/sniffercommit/main/install.sh | sh -s -- v0.3.9

set -eu

REPO="slowy07/sniffercommit"
VERSION="latest"
FORCE_BUILD=false
VERBOSE=false
NO_MODIFY_PATH=false
UNINSTALL=false

# Parse arguments
for arg in "$@"; do
    case "${arg}" in
        --help | -h)
            echo "sniffercommit install script"
            echo ""
            echo "Usage:"
            echo "  curl -LsSf https://raw.githubusercontent.com/slowy07/sniffercommit/main/install.sh | sh"
            echo "  curl -LsSf https://raw.githubusercontent.com/slowy07/sniffercommit/main/install.sh | sh -s -- --force"
            echo "  curl -LsSf https://raw.githubusercontent.com/slowy07/sniffercommit/main/install.sh | sh -s -- v0.3.9"
            exit 0
            ;;
        --version)
            echo "sniffercommit 0.4.0"
            exit 0
            ;;
        --verbose) VERBOSE=true ;;
        --force | -f | --force-build) FORCE_BUILD=true ;;
        --no-modify-path) NO_MODIFY_PATH=true ;;
        --uninstall) UNINSTALL=true ;;
        *)
            if echo "${arg}" | grep -q '^[^-]'; then
                VERSION="${arg}"
            fi
            ;;
    esac
done

if [ -n "${SNIFFERCOMMIT_FORCE_BUILD:-}" ]; then
    FORCE_BUILD=true
fi

if [ -n "${CARGO_HOME:-}" ]; then
    DEFAULT_INSTALL_DIR="${CARGO_HOME}/bin"
else
    DEFAULT_INSTALL_DIR="${HOME}/.local/bin"
fi
INSTALL_DIR="${SNIFFERCOMMIT_INSTALL_DIR:-${SNIFFER_INSTALL_DIR:-${DEFAULT_INSTALL_DIR}}}"

if [ "${UNINSTALL}" = "true" ]; then
    if [ -f "${INSTALL_DIR}/sniffercommit" ]; then
        rm -f "${INSTALL_DIR}/sniffercommit"
        echo "sniffercommit uninstalled from ${INSTALL_DIR}/sniffercommit"
    else
        echo "sniffercommit not found at ${INSTALL_DIR}/sniffercommit" >&2
    fi
    exit 0
fi

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

if [ "${FORCE_BUILD}" = "true" ]; then
    [ "${VERBOSE}" = "true" ] && echo "Building sniffercommit from source..."

    BUILD_DIR="$(mktemp -d)"
    trap 'rm -rf "${BUILD_DIR}"' EXIT

    BRANCH="${SNIFFERCOMMIT_BRANCH:-main}"
    [ "${VERBOSE}" = "true" ] && echo "Cloning ${REPO} branch ${BRANCH}..."
    if ! git clone --depth 1 --branch "${BRANCH}" "https://github.com/${REPO}.git" "${BUILD_DIR}/src" 2>/dev/null; then
        echo "Error: Failed to clone repository" >&2
        exit 1
    fi

    [ "${VERBOSE}" = "true" ] && echo "Configuring CMake..."
    (cd "${BUILD_DIR}/src" && cmake -B build -DCMAKE_BUILD_TYPE=Release -DSNIFFERCOMMIT_BUILD_TESTS=OFF)
    if [ $? -ne 0 ]; then
        echo "Error: CMake configuration failed" >&2
        exit 1
    fi

    [ "${VERBOSE}" = "true" ] && echo "Building..."
    (cd "${BUILD_DIR}/src" && cmake --build build --parallel)
    if [ $? -ne 0 ]; then
        echo "Error: Build failed" >&2
        exit 1
    fi

    mkdir -p "${INSTALL_DIR}"
    cp "${BUILD_DIR}/src/build/bin/sniffercommit" "${INSTALL_DIR}/sniffercommit" 2>/dev/null ||
        cp "${BUILD_DIR}/src/build/sniffercommit" "${INSTALL_DIR}/sniffercommit"
    chmod +x "${INSTALL_DIR}/sniffercommit"

    echo "sniffercommit installed to ${INSTALL_DIR}/sniffercommit"
else
    [ "${VERBOSE}" = "true" ] && echo "Downloading sniffercommit for ${PLATFORM}..."

    TEMP_DIR="$(mktemp -d)"
    trap 'rm -rf "${TEMP_DIR}"' EXIT

    DOWNLOAD_FILE="${TEMP_DIR}/sniffercommit.tar.gz"

    if command -v curl >/dev/null 2>&1; then
        [ "${VERBOSE}" = "true" ] && echo "Using curl..."
        curl -LsSf "${DOWNLOAD_URL}" -o "${DOWNLOAD_FILE}" || download_failed=1
    elif command -v wget >/dev/null 2>&1; then
        [ "${VERBOSE}" = "true" ] && echo "Using wget..."
        wget -q "${DOWNLOAD_URL}" -O "${DOWNLOAD_FILE}" || download_failed=1
    else
        echo "Error: need curl or wget"
        exit 1
    fi

    if [ "${download_failed:-0}" -ne 0 ]; then
        echo "Warning: Failed to download sniffercommit from ${DOWNLOAD_URL}" >&2
        echo "Warning: No release found. Use --force to build from source." >&2
        exit 0
    fi

    tar xzf "${DOWNLOAD_FILE}" -C "${TEMP_DIR}"

    mkdir -p "${INSTALL_DIR}"
    cp "${TEMP_DIR}/sniffercommit" "${INSTALL_DIR}/sniffercommit"
    chmod +x "${INSTALL_DIR}/sniffercommit"

    echo "sniffercommit installed to ${INSTALL_DIR}/sniffercommit"
fi

if [ "${NO_MODIFY_PATH}" = "false" ]; then
    case ":${PATH}:" in
        *:"${INSTALL_DIR}":*) ;;
        *)
            echo ""
            echo "  ${INSTALL_DIR} is not in your PATH."
            echo "  Add this to your shell profile:"
            echo "    export PATH=\"\${HOME}/.local/bin:\${PATH}\""
            ;;
    esac
fi

echo "Run 'sniffercommit --help' to get started."
