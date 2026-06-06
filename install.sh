#!/bin/sh
# shellcheck shell=sh disable=SC3043
# curl -LsSf https://raw.githubusercontent.com/slowy07/sniffercommit/develop/install.sh | sh

set -u

APP_NAME="sniffercommit"
REPO_OWNER="slowy07"
REPO_NAME="sniffercommit"
REPO_URL="https://github.com/${REPO_OWNER}/${REPO_NAME}"
RELEASE_API="https://api.github.com/repos/${REPO_OWNER}/${REPO_NAME}/releases"

SCRIPT_VERSION="0.3.3"
APP_VERSION="${SNIFFERCOMMIT_VERSION:-latest}"
INSTALL_BRANCH="${SNIFFERCOMMIT_BRANCH:-develop}"
FORCE_BUILD="${SNIFFERCOMMIT_FORCE_BUILD:-0}"
PREFER_BUILD="${SNIFFERCOMMIT_PREFER_BUILD:-0}"

NO_MODIFY_PATH="${SNIFFERCOMMIT_NO_MODIFY_PATH:-0}"
PRINT_VERBOSE="${SNIFFERCOMMIT_PRINT_VERBOSE:-0}"
PRINT_QUIET="${SNIFFERCOMMIT_PRINT_QUIET:-0}"

say() {
    if [ "${PRINT_QUIET}" = "0" ]; then
        printf '%s\n' "$1"
    fi
}

say_verbose() {
    if [ "${PRINT_VERBOSE}" = "1" ]; then
        printf '[verbose] %s\n' "$1"
    fi
}

warn() {
    printf '[WARN] %s\n' "$1" >&2
}

err() {
    printf '[ERROR] %s\n' "$1" >&2
    exit 1
}

usage() {
    cat <<EOF
${APP_NAME}-installer.sh

The installer for ${APP_NAME}

This script detects what platform you're on and fetches an appropriate archive
from ${REPO_URL}/releases, then unpacks the binary and installs it to
the first of the following locations:

  \$CARGO_HOME/bin
  \$HOME/.cargo/bin
  \$HOME/.local/bin

It will then add that dir to PATH by adding the appropriate line to your shell profiles.

USAGE:
    ${APP_NAME}-installer.sh [OPTIONS]

OPTIONS:
    -v, --verbose
        Enable verbose output

    -q, --quiet
        Disable progress output

    --no-modify-path
        Don't configure the PATH environment variable

    --force-build
        Force build from source instead of downloading release binary

    --enable-clang-tidy
        Enable clang-tidy configuration generation when building from source

    --enable-cmake
        Enable CMakeLists.txt scaffolding when building from source

    --generate-src
        Generate src/main.cpp scaffold when building from source

    -V, --version
        Print version information

    -h, --help
        Print help information
EOF
}

need_cmd() {
    if ! check_cmd "$1"; then
        err "need '$1' (command not found)"
    fi
}

check_cmd() {
    command -v "$1" >/dev/null 2>&1
}

get_architecture() {
    local _ostype _cputype _arch

    _ostype="$(uname -s)"
    _cputype="$(uname -m)"

    case "$_ostype" in
        Linux)
            _ostype="Linux"
            ;;
        Darwin)
            _ostype="darwin"
            ;;
        FreeBsd)
            _ostype="freebsd"
            ;;
        MINGW* | MSYS* | CYGWIN*)
            _ostype="windows"
            ;;
        *)
            err "unssupported os type: $_ostype"
            ;;
    esac

    _arch="${_cputype}-${_ostype}"
    RETVAL="$_arch"
}

downloader() {
    local _dld

    if check_cmd curl; then
        _dld=curl
    elif check_cmd wget; then
        _dld=wget
    else
        _dld='fetch'
    fi

    if [ "$1" = --check ]; then
        need_cmd "$_dld"
    elif [ "$_dld" = curl ]; then
        curl --proto '=https' --tlsv1.2 -sSfL "$1" -o "$2"
    elif [ "$_dld" = wget ]; then
        wget "$1" -O "$2"
    else
        err "no downloader found (need curl or wget)"
    fi
}

get_latest_version() {
    say_verbose "Fetching latest release info from Github API"
    local _release
    _release="$(downloader "${RELEASE_API}/latest" -)"
    echo "$_release" | grep -o '"tag_name": *"[^"]*"' | grep -o '"[^"]*"$' | tr -d '"'
}

get_release_asset_url() {
    local _version="$1"
    local _arch="$2"

    if [ "$_version" = "latest" ]; then
        _version="$(get_latest_version)"
        if [ -z "$_version" ]; then
            return 1
        fi
    fi

    say_verbose "Resolving version: $_version for arch: $_arch"
    local _asset_pattern="${APP_NAME}-.*-${_arch}\\.tar\\.gz"
    local _release_url="${RELEASE_API}/tags/${_version}"
    local _release_json

    _release_json="$(downloader "$_release_url" -)"

    local _url
    _url="$(echo "$_release_json" | grep -o '"browser_download_url": *"[^"]*"' | grep "${_arch}" | head -n 1 | grep -o '"[^"]*"$' | tr -d '"')"

    if [ -n "$_url" ]; then
        echo "$_url"
        return 0
    fi

    _url="$(echo "$_release_json" | grep -o '"browser_download_url": *"[^"]*\.tar\.gz"' | head -n 1 | grep -o '"[^"]*"$' | tr -d '"')"

    if [ -n "$_url" ]; then
        echo "$_url"
        return 0
    fi

    return 1
}

install_from_release() {
    local _arch="$1"
    local _version="$2"
    local _install_dir="$3"

    say "Downloading ${APP_NAME} ${_version} for ${_arch}..."

    local _url
    _url="$(get_release_asset_url "$_version" "$_arch")"
    if [ -z "$_url" ]; then
        return 1
    fi

    say_verbose "Download URL: $_url"

    local _tmp_dir
    _tmp_dir="$(mktemp -d)"
    local _archive="${_tmp_dir}/${APP_NAME}.tar.gz"

    downloader "$_url" "$_archive"

    say "Extracting..."
    tar -xzf "$_archive" -C "$_tmp_dir"

    local _binary
    _binary="$(find "$_tmp_dir" -type f -name "${APP_NAME}" | head -n 1)"

    if [ -z "$_binary" ]; then
        _binary="$(find "$_tmp_dir" -type f -name "${APP_NAME}.exe" | head -n 1)"
    fi

    if [ -z "$_binary" ]; then
        rm -rf "$_tmp_dir"
        err "Could not find ${APP_NAME} binary in downloaded archive"
    fi

    say "Installing to ${_install_dir}..."
    mkdir -p "$_install_dir"
    cp "$_binary" "${_install_dir}/${APP_NAME}"
    chmod +x "${_install_dir}/${APP_NAME}"

    rm -rf "$_tmp_dir"

    say "${APP_NAME} ${_version} installed successfully!"
    return 0
}

install_from_source() {
    local _install_dir="$1"
    local _enable_clang_tidy="${2:-0}"
    local _enable_cmake="${3:-0}"
    local _generate_src="${4:-0}"

    say "Building ${APP_NAME} from source"

    need_cmd cmake
    need_cmd git

    local _cmake_version
    _cmake_version="$(cmake --version | head -n 1)"
    say "Found: $_cmake_version"

    local _generator
  if check_cmd ninja; then
    _generator="Ninja"
  elif check_cmd gmake || check_cmd make; then
    _generator="Unix Makefiles"
  fi

    if [ -z "$_generator" ]; then
        err "No suitable build tool found (need ninja or make)"
    fi

    say "Using generator: $_generator"

    local _tmp_dir
    _tmp_dir="$(mktemp -d)"
    local _source_dir="${_tmp_dir}/${REPO_NAME}"

    say "Cloning repository (branch: ${INSTALL_BRANCH})..."
    git clone --depth 1 --branch "${INSTALL_BRANCH}" "${REPO_URL}.git" "$_source_dir"

    local _build_dir="${_source_dir}/build"
    mkdir -p "$_build_dir"

    say "Configuring with CMake..."
    set -- \
        -S "$_source_dir" \
        -B "$_build_dir" \
        -G "$_generator" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

    if [ "$_enable_clang_tidy" = "1" ]; then
        set -- "$@" -DSNIFFERCOMMIT_ENABLE_CLANG_TIDY=ON
    fi
    if [ "$_enable_cmake" = "1" ]; then
        set -- "$@" -DSNIFFERCOMMIT_ENABLE_CMAKE=ON
    fi
    if [ "$_generate_src" = "1" ]; then
        set -- "$@" -DSNIFFERCOMMIT_GENERATE_SRC=ON
    fi

    cmake "$@"

    say "Building..."
    cmake --build "$_build_dir" --parallel

    local _built_binary="${_build_dir}/bin/${APP_NAME}"
    if [ ! -f "$_built_binary" ]; then
        _built_binary="${_build_dir}/${APP_NAME}"
    fi
    if [ ! -f "$_built_binary" ]; then
        _built_binary="${_build_dir}/Release/${APP_NAME}"
    fi
    if [ ! -f "$_built_binary" ]; then
        _built_binary="$(find "$_build_dir" -type f -name "${APP_NAME}" 2>/dev/null | head -n 1)"
    fi
    if [ ! -f "$_built_binary" ]; then
        rm -rf "$_tmp_dir"
        err "Could not find built executable"
    fi

    say "Installing to ${_install_dir}..."
    mkdir -p "$_install_dir"
    cp "$_built_binary" "${_install_dir}/${APP_NAME}"
    chmod +x "${_install_dir}/${APP_NAME}"

    rm -rf "$_tmp_dir"

    say "${APP_NAME} built and installed successfully!"
}

get_install_dir() {
    # Priority: CARGO_HOME/bin > HOME/.cargo/bin > HOME/.local/bin
    if [ -n "${CARGO_HOME:-}" ]; then
        echo "${CARGO_HOME}/bin"
        return
    fi
    if [ -n "${HOME:-}" ]; then
        if [ -d "${HOME}/.cargo/bin" ]; then
            echo "${HOME}/.cargo/bin"
            return
        fi
        echo "${HOME}/.local/bin"
        return
    fi
    err "could not determine install directory (set \$CARGO_HOME or \$HOME)"
}

add_to_path() {
    local _install_dir="$1"

    if [ "${NO_MODIFY_PATH}" = "1" ]; then
        say_verbose "Skipping PATH modification (--no-modify-path)"
        return
    fi

    case ":${PATH}:" in
        *":${_install_dir}:"*)
            say_verbose "${_install_dir} is already in PATH"
            return
            ;;
    esac

    local _shell="${SHELL:-}"
    local _rcfile=""

    case "$_shell" in
        */fish)
            _rcfile="${HOME}/.config/fish/config.fish"
            ;;
        */zsh)
            _rcfile="${HOME}/.zshrc"
            ;;
        */bash | */sh | *)
            _rcfile="${HOME}/.bashrc"
            if [ -f "${HOME}/.bash_profile" ]; then
                _rcfile="${HOME}/.bash_profile"
            fi
            ;;
    esac

    if [ -n "$_rcfile" ] && [ -f "$_rcfile" ]; then
        if ! grep -q "${_install_dir}" "$_rcfile" 2>/dev/null; then
            say "Adding ${_install_dir} to PATH in ${_rcfile}"
            printf '\n# Added by %s installer\nexport PATH="%s:$PATH"\n' "$APP_NAME" "$_install_dir" >>"$_rcfile"
        fi
    fi

    say ""
    say "To use ${APP_NAME} immediately, run:"
    say "  export PATH=\"${_install_dir}:\$PATH\""
    say "Or restart your terminal."
}

uninstall() {
    local _install_dir
    _install_dir="$(get_install_dir)"
    local _binary="${_install_dir}/${APP_NAME}"

    if [ -f "$_binary" ]; then
        say "Removing ${_binary}"
        rm -f "$_binary"
    else
        err "${APP_NAME} not found at ${_binary}"
    fi

    for _rcfile in "${HOME}/.bashrc" "${HOME}/.bash_profile" "${HOME}/.zshrc" "${HOME}/.config/fish/config.fish"; do
        if [ -f "$_rcfile" ]; then
            sed -i.bak "/# Added by ${APP_NAME} installer/d" "$_rcfile" 2>/dev/null || true
            sed -i.bak "/export PATH=\"${_install_dir}:\$PATH\"/d" "$_rcfile" 2>/dev/null || true
            rm -f "${_rcfile}.bak"
        fi
    done

    say "${APP_NAME} uninstalled successfully"
}

main() {
    local _uninstall=0
    local _enable_clang_tidy=0
    local _enable_cmake=0
    local _generate_src=0

    for arg in "$@"; do
        case "$arg" in
            -h | --help)
                usage
                exit 0
                ;;
            -V | --version)
                printf '%s-installer %s\n' "$APP_NAME" "$SCRIPT_VERSION"
                exit 0
                ;;
            -v | --verbose)
                PRINT_VERBOSE=1
                ;;
            -q | --quiet)
                PRINT_QUIET=1
                ;;
            --no-modify-path)
                NO_MODIFY_PATH=1
                ;;
            --force-build)
                FORCE_BUILD=1
                ;;
            --enable-clang-tidy)
                _enable_clang_tidy=1
                ;;
            --enable-cmake)
                _enable_cmake=1
                ;;
            --generate-src)
                _generate_src=1
                ;;
            --uninstall)
                _uninstall=1
                ;;
            *)
                err "unknown option: $arg"
                ;;
        esac
    done

    if [ "${PRINT_QUIET}" = "0" ]; then
        printf '%s\n' "${APP_NAME}"
        printf 'Installer for %s\n' "$REPO_URL"
        printf '\n'
    fi

    if [ "$_uninstall" = "1" ]; then
        uninstall
        exit 0
    fi

    local _install_dir
    _install_dir="$(get_install_dir)"
    local _existing="${_install_dir}/${APP_NAME}"

    if [ -f "$_existing" ] && [ "${FORCE_BUILD}" = "0" ]; then
        warn "${APP_NAME} is already installed"
        say "Use --force-build to reinstall or --uninstall to remove"
        exit 0
    fi

    get_architecture
    local _arch="$RETVAL"
    say "Platform: ${_arch}"

    local _installed=0

    if [ "${FORCE_BUILD}" = "0" ] && [ "${PREFER_BUILD}" = "0" ]; then
        if install_from_release "$_arch" "$APP_VERSION" "$_install_dir"; then
            _installed=1
        else
            say "No prebuilt binary found, falling back to source build..."
        fi
    fi

    if [ "$_installed" = "0" ]; then
        if [ "${PREFER_BUILD}" = "1" ]; then
            say "Preferring build from source (SNIFFERCOMMIT_PREFER_BUILD=1)"
        elif [ "${FORCE_BUILD}" = "1" ]; then
            say "Forced build from source (SNIFFERCOMMIT_FORCE_BUILD=1)"
        fi
        install_from_source "$_install_dir" "$_enable_clang_tidy" "$_enable_cmake" "$_generate_src"
    fi

    add_to_path "$_install_dir"

    if [ -f "${_install_dir}/${APP_NAME}" ]; then
        say ""
        say "Run '${APP_NAME} --help' to get started"
    fi
}

main "$@"
