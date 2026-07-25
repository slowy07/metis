#!/bin/sh
# sniffercommit install script
#
# Usage:
#   curl -LsSf https://raw.githubusercontent.com/slowy07/sniffercommit/main/install.sh | sh
#   curl -LsSf https://raw.githubusercontent.com/slowy07/sniffercommit/main/install.sh | sh -s -- v0.3.15
#   curl -LsSf https://raw.githubusercontent.com/slowy07/sniffercommit/main/install.sh | sh -s -- --force
set -eu

REPO="slowy07/sniffercommit"
VERSION="latest"
FORCE_BUILD=false
VERBOSE=false
NO_MODIFY_PATH=false
UNINSTALL=false

say() {
  printf '\033[1minstall.sh:\033[0m %s\n' "$1"
}

say_verbose() {
  if [ "${VERBOSE}" = "true" ]; then
    printf 'install.sh: %s\n' "$1"
  fi
}

warn() {
  printf 'install.sh: \033[33mwarning:\033[0m %s\n' "$1" >&2
}

err() {
  printf 'install.sh: \033[31merror:\033[0m %s\n' "$1" >&2
  exit 1
}

need_cmd() {
  if ! command -v "$1" >/dev/null 2>&1; then
    err "required command '$1' not found on PATH"
  fi
}

for arg in "$@"; do
  case "${arg}" in
  --help | -h)
    cat <<'EOF'
sniffercommit install script

Usage:
  curl -LsSf https://raw.githubusercontent.com/slowy07/sniffercommit/main/install.sh | sh
  curl -LsSf https://raw.githubusercontent.com/slowy07/sniffercommit/main/install.sh | sh -s -- --force
  curl -LsSf https://raw.githubusercontent.com/slowy07/sniffercommit/main/install.sh | sh -s -- v0.3.15

Options:
  -h, --help              Print this help and exit
  --version               Print the installer's own version and exit
  --verbose               Print extra diagnostic output
  -f, --force,
      --force-build       Rebuild from source even if a prebuilt binary exists
  --no-modify-path        Don't attempt to add the install dir to PATH
  --uninstall             Remove a previously installed sniffercommit
  <version>               Install a specific tag, e.g. v0.3.15 (default: latest)

Environment variables:
  SNIFFERCOMMIT_INSTALL_DIR   Override the install directory
  SNIFFERCOMMIT_FORCE_BUILD   Same as --force if set to a non-empty value
EOF
    exit 0
    ;;
  --version)
    echo "sniffercommit installer 0.3.15"
    exit 0
    ;;
  --verbose) VERBOSE=true ;;
  --force | -f | --force-build) FORCE_BUILD=true ;;
  --no-modify-path) NO_MODIFY_PATH=true ;;
  --uninstall) UNINSTALL=true ;;
  -*)
    err "unknown flag '${arg}' (see --help)"
    ;;
  *)
    VERSION="${arg}"
    ;;
  esac
done

if [ -n "${SNIFFERCOMMIT_FORCE_BUILD:-}" ]; then
  FORCE_BUILD=true
fi

if [ -n "${SNIFFERCOMMIT_INSTALL_DIR:-}" ]; then
  DEFAULT_INSTALL_DIR="${SNIFFERCOMMIT_INSTALL_DIR}"
else
  DEFAULT_INSTALL_DIR="${HOME}/.local/bin"
fi
INSTALL_DIR="${DEFAULT_INSTALL_DIR}"
BIN_NAME="sniffercommit"

do_uninstall() {
  target="${INSTALL_DIR}/${BIN_NAME}"
  if [ -f "${target}" ]; then
    rm -f "${target}"
    say "removed ${target}"
  else
    warn "no installation found at ${target}"
  fi
  exit 0
}

if [ "${UNINSTALL}" = "true" ]; then
  do_uninstall
fi

detect_os() {
  _os="$(uname -s)"
  case "${_os}" in
  Linux) echo "unknown-linux-gnu" ;;
  Darwin) echo "apple-darwin" ;;
  MINGW* | MSYS* | CYGWIN*) echo "pc-windows-msvc" ;;
  *) err "unsupported OS: ${_os}" ;;
  esac
}

detect_arch() {
  _arch="$(uname -m)"
  case "${_arch}" in
  x86_64 | amd64) echo "x86_64" ;;
  aarch64 | arm64) echo "aarch64" ;;
  *) err "unsupported architecture: ${_arch}" ;;
  esac
}

OS="$(detect_os)"
ARCH="$(detect_arch)"
TARGET="${ARCH}-${OS}"
say_verbose "detected target: ${TARGET}"

if command -v curl >/dev/null 2>&1; then
  DOWNLOADER="curl"
elif command -v wget >/dev/null 2>&1; then
  DOWNLOADER="wget"
else
  err "need either 'curl' or 'wget' installed to download sniffercommit"
fi

fetch() {
  # fetch <url> <output_path>
  _url="$1"
  _out="$2"
  if [ "${DOWNLOADER}" = "curl" ]; then
    curl -LsSf "${_url}" -o "${_out}"
  else
    wget -q "${_url}" -O "${_out}"
  fi
}

fetch_stdout() {
  _url="$1"
  if [ "${DOWNLOADER}" = "curl" ]; then
    curl -LsSf "${_url}"
  else
    wget -qO- "${_url}"
  fi
}

if [ "${VERSION}" = "latest" ]; then
  say_verbose "resolving latest release tag"
  RESOLVED_TAG="$(fetch_stdout "https://api.github.com/repos/${REPO}/releases/latest" |
    grep '"tag_name":' |
    sed -E 's/.*"tag_name": *"([^"]+)".*/\1/')"
  [ -n "${RESOLVED_TAG}" ] || err "could not resolve latest release tag from GitHub API"
  VERSION="${RESOLVED_TAG}"
fi
say "installing sniffercommit ${VERSION} (${TARGET})"

need_cmd mktemp
WORK_DIR="$(mktemp -d)"
cleanup() {
  rm -rf "${WORK_DIR}"
}
trap cleanup EXIT INT TERM

install_prebuilt() {
  ARCHIVE_EXT="tar.gz"
  case "${OS}" in
  pc-windows-msvc) ARCHIVE_EXT="zip" ;;
  esac

  ASSET="sniffercommit-${VERSION}-${TARGET}.${ARCHIVE_EXT}"
  ASSET_URL="https://github.com/${REPO}/releases/download/${VERSION}/${ASSET}"
  CHECKSUM_URL="${ASSET_URL}.sha256"

  say_verbose "downloading ${ASSET_URL}"
  if ! fetch "${ASSET_URL}" "${WORK_DIR}/${ASSET}" 2>/dev/null; then
    return 1
  fi

  if fetch "${CHECKSUM_URL}" "${WORK_DIR}/${ASSET}.sha256" 2>/dev/null; then
    say_verbose "verifying checksum"
    (
      cd "${WORK_DIR}" &&
        if command -v sha256sum >/dev/null 2>&1; then
          sha256sum -c "${ASSET}.sha256"
        elif command -v shasum >/dev/null 2>&1; then
          shasum -a 256 -c "${ASSET}.sha256"
        else
          warn "no sha256 tool found, skipping checksum verification"
        fi
    ) || err "checksum verification failed for ${ASSET}"
  else
    warn "no checksum file found for this release, skipping verification"
  fi

  say_verbose "extracting archive"
  if [ "${ARCHIVE_EXT}" = "zip" ]; then
    need_cmd unzip
    unzip -q "${WORK_DIR}/${ASSET}" -d "${WORK_DIR}/extracted"
  else
    mkdir -p "${WORK_DIR}/extracted"
    tar -xzf "${WORK_DIR}/${ASSET}" -C "${WORK_DIR}/extracted"
  fi

  _found_bin="$(find "${WORK_DIR}/extracted" -type f -name "${BIN_NAME}*" | head -n1)"
  [ -n "${_found_bin}" ] || err "could not find '${BIN_NAME}' binary inside downloaded archive"

  mkdir -p "${INSTALL_DIR}"
  install -m 755 "${_found_bin}" "${INSTALL_DIR}/${BIN_NAME}"
  return 0
}

install_from_source() {
  say "building from source (this requires cmake and a C++20 compiler)"
  need_cmd cmake
  need_cmd make
  (cd "${WORK_DIR}" &&
    git clone --depth 1 --branch "${VERSION}" "https://github.com/${REPO}.git" src &&
    cd src &&
    mkdir -p build &&
    cd build &&
    cmake .. -DCMAKE_BUILD_TYPE=Release -DSNIFFERCOMMIT_BUILD_TESTS=OFF &&
    cmake --build . --parallel "$(nproc)")
  mkdir -p "${INSTALL_DIR}"
  install -m 755 "${WORK_DIR}/src/build/bin/${BIN_NAME}" "${INSTALL_DIR}/${BIN_NAME}"
}

if [ "${FORCE_BUILD}" = "true" ]; then
  install_from_source
else
  if ! install_prebuilt; then
    warn "no prebuilt binary available for ${TARGET}, falling back to source build"
    install_from_source
  fi
fi

say "installed ${BIN_NAME} to ${INSTALL_DIR}/${BIN_NAME}"

case ":${PATH}:" in
*":${INSTALL_DIR}:"*)
  ALREADY_ON_PATH=true
  ;;
*)
  ALREADY_ON_PATH=false
  ;;
esac

if [ "${ALREADY_ON_PATH}" = "false" ] && [ "${NO_MODIFY_PATH}" = "false" ]; then
  SHELL_NAME="$(basename "${SHELL:-sh}")"
  case "${SHELL_NAME}" in
  zsh) PROFILE="${HOME}/.zshrc" ;;
  bash) PROFILE="${HOME}/.bashrc" ;;
  fish) PROFILE="${HOME}/.config/fish/config.fish" ;;
  *) PROFILE="${HOME}/.profile" ;;
  esac

  LINE="export PATH=\"${INSTALL_DIR}:\$PATH\""
  if [ "${SHELL_NAME}" = "fish" ]; then
    LINE="set -gx PATH ${INSTALL_DIR} \$PATH"
  fi

  if [ -f "${PROFILE}" ] && grep -qF "${INSTALL_DIR}" "${PROFILE}" 2>/dev/null; then
    say_verbose "${PROFILE} already references ${INSTALL_DIR}"
  else
    printf '\n# added by sniffercommit installer\n%s\n' "${LINE}" >>"${PROFILE}"
    say "added ${INSTALL_DIR} to PATH in ${PROFILE}"
    say "run 'source ${PROFILE}' or start a new shell to use ${BIN_NAME}"
  fi
elif [ "${ALREADY_ON_PATH}" = "false" ]; then
  warn "${INSTALL_DIR} is not on your PATH; add it manually to run '${BIN_NAME}'"
fi

say "done. run '${BIN_NAME} --help' to get started"
