[![GitHub Release](https://img.shields.io/github/v/release/slowy07/metis?display_name=tag&style=flat-square)](https://github.com/slowy07/metis/releases)

[![GitHub Actions Workflow Status](https://img.shields.io/github/actions/workflow/status/slowy07/metis/test_metis.yml?branch=develop&style=flat-square&label=GoogleTest)
](https://github.com/slowy07/metis/actions)

Fast, C++20-powered pre-commit hook and CI generator. Ensures code quality before every push with zero runtime dependencies: a single static binary that replaces Python- and Node-based pre-commit frameworks.

## Features

- **Parallel check execution** : run linters concurrently via Bash background jobs
- **Pattern-aware filtering** : apply checks only to matching file extensions (`.cpp`, `.hpp`, etc.)
- **Zero runtime dependencies** : single static binary; no Python/Node required
- **Auto-generate CI workflows** : GitHub Actions and GitLab CI workflows mirroring local hooks
- **.clang-format scaffolding** : generate `.clang-format` with configurable style presets
- **.clang-tidy** integration : static analysis with curated check preset (`minimal`, `standard`, `strict`)
- **CMake scaffolding** : generate `CMakeLists.txt` with compiler warnings, sanitizers, clang-tidy/clang-format integration, and testing
- **Conan support** : generate `conanfile.py` alongside CMakeLists.txt for Conan package manager integration
- **Hook syntax validation** : generated bash hooks are validated with `bash -n` before install
- **Git worktree support** : works in worktrees, submodules, and detached checkouts

## Install

```bash
# POSIX (Linux, macOS)
curl -LsSf https://raw.githubusercontent.com/slowy07/metis/develop/install.sh | sh
```

```powershell
# Windows (PowerShell)
irm https://raw.githubusercontent.com/slowy07/metis/develop/install.ps1 | iex
```

The script detects your platform, downloads the latest release binary, and adds it to your `PATH`.

## Quick Start

```bash
cd my-project

# Initialize config and tooling files
metis init --style google --enable-clang-tidy

# Install the pre-commit hook (auto-runs on every `git commit`)
metis install

# Stage and commit: hook fires automatically
git add .
git commit -m "initial"

# Or run checks ad-hoc without committing
metis run --all-files
```

`exit code 0` → all checks pass. Non-zero → at least one check failed.

## Subcommands

| Command | Action |
|---------|--------|
| `init` | Generate `.metis.toml`, `.clang-format` (plus `.clang-tidy` / `CMakeLists.txt` with `--enable-*` flags) |
| `install` | Install pre-commit hook + optional CI workflow |
| `run` | Execute checks on files (staged by default, `--all-files` for tracked) |
| `generate-gha` | Write GitHub Actions workflow to `.github/workflows/metis.yml` |
| `generate-gitlab` | Write GitLab CI workflow to `.gitlab-ci.yml` |
| `install-compiler` | Download and install a C++ toolchain |
| `test` | Run ctest and optional coverage checks |
| `sanitizer` | Run sanitizer checks (ASan, UBSan, TSan, LSan) |
| `deps` | Check project dependencies (Conan, vcpkg, CMake FetchContent) |
| `perf` | Run performance checks (build time, binary size, benchmarks); bare `metis perf` runs all enabled checks |
| `--version`, `-v` | Print version and exit |
| `--help` | Print help and exit |

Global flag: `--config <path>`: use a non-default config file path with any subcommand.

## Dependencies

| Dependency | Purpose | Version |
| ------------- | -------------- | -------------- |
| [tomlplusplus](https://github.com/marzer/tomlplusplus.git) | Header-only TOML config file parser and serializer | `v3.4.0` |
| [fmt](https://github.com/fmtlib/fmt.git) | Modern formatting library | `v11.0.2` |

Both are fetched automatically at configure time via CMake's `FetchContent`. No system installation required.

Optional system packages (if `METIS_USE_SYSTEM_FMT=ON`):

```bash
# macOS (Homebrew)
brew install fmt
```

## Build

```bash
# Release build (recommended)
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel

# Verify
./build/metis --version

# Quick smoke test
./build/metis init --style google --enable-clang-tidy
./build/metis run --dry-run --all-files
./build/metis install
```

| Option | Default | Description |
| ----- | ------- | ----------- |
| `METIS_ENABLE_SANITIZERS` | `OFF` | AddressSanitizer + UBSan (Debug only) |
| `METIS_USE_SYSTEM_FMT` | `ON` | Use system `libfmt` instead of FetchContent |
| `METIS_USE_SYSTEM_TOMLPLUSPLUS` | `OFF` | Use system `tomlplusplus` instead of FetchContent |

**Compiler Requirements**

- `GCC >= 11`
- `Clang >= 14`
- `MSVC >= 2022 17.0`

**Install (from build)**

```bash
sudo cmake --install build
```

## Usage

### init: Generate Configuration

**Interactive mode (default)**

Running `metis init` with no flags will launch interactive wizard:

```bash
cd /path/to/project
metis init
```
How it works:
    - Press enter to acept the detault value who in `[bracket value info]`
    - Type value to overriding
    - Invalid input is rejected with a warning; and the prompt are repeats

```bash
metis init --interactive # or using -i
```
if using more flag like `metis --style google` or another flag after `init` will disable interactive wizard and using CLI-only mode.


**With clang-tidy (static analysis)**

```bash
metis init --enable-clang-tidy                        # standard preset
metis init --enable-clang-tidy --tidy-preset strict   # strict preset
metis init --enable-clang-tidy --tidy-severity warning # compiler warnings only
```

With `--enable-clang-tidy`, three files are created:
- `.metis.toml`: includes `clang-tidy` check
- `.clang-format`: formatter config
- `.clang-tidy`: static analysis config with curated check preset

**Formatter & Analyzer Options**

```bash
# available styles: google, llvm, chromium, mozilla, webkit, microsoft, gnu
metis init --style llvm

# clang-tidy preset: minimal, standard (default), strict, custom
metis init --enable-clang-tidy --tidy-preset standard

# tidy severity: note (off), warning (compiler only), error (all)
metis init --enable-clang-tidy --tidy-severity warning

# header filter: 0=none, 1=project, 2=all
metis init --enable-clang-tidy --tidy-header-filter 1

# full customization
metis init \
  --style google \
  --name my-project \
  --indent-width 4 \
  --column-limit 120 \
  --pointer-alignment Left \
  --brace-style Attach \
  --enable-clang-tidy \
  --tidy-preset strict \
  --tidy-severity error
```

**Custom config path**

```bash
metis --config /path/to/.metis.toml init
```

---

### install: Install Pre-commit Hook

```bash
metis install
```

Installs:
- `.git/hooks/pre-commit`: Bash hook with parallel execution
- `.github/workflows/metis.yml`: GitHub Actions workflow (only if `github_actions = true` in config)
- `.gitlab-ci.yml`: GitLab CI workflow (only if `gitlab_ci = true` in config)

The hook is validated with `bash -n` before installation to prevent broken hooks.

**Uninstall**

```bash
rm .git/hooks/pre-commit
rm -rf .github/workflows/metis.yml
rm -f .gitlab-ci.yml
```

Or re-run `metis install` to regenerate both files from config.

---

### run: Execute Checks Manually

```bash
# staged files only (default)
metis run

# all tracked files
metis run --all-files

# specific files
metis run src/main.cpp include/foo.hpp

# dry-run (list files without executing)
metis run --dry-run --all-files

# verbose (print each shell command)
metis run --verbose --all-files
```

Exit code: `0` if all checks pass, non-zero if any check fails.

---

### generate-gha: CI Workflow Only

```bash
metis generate-gha
```

Always writes `.github/workflows/metis.yml` regardless of the `github_actions` config setting.

---

### generate-gitlab: GitLab CI Workflow Only

```bash
metis generate-gitlab
```

Always writes `.gitlab-ci.yml` regardless of the `gitlab_ci` config setting.

---

### Tidy Presets

Used with `metis init --enable-clang-tidy`:

| Preset | Scope |
|--------|-------|
| `minimal` | cppcoreguidelines + bugprone + clang-analyzer |
| `standard` (default) | minimal + modernize + performance |
| `strict` | All checks minus noisy ones (abseil, altera, fuchsia, llvm, zircon) |
| `custom` | User-defined via `extra_checks` / `exclude_checks` |

---

### Manual clang-tidy (no metis wrapper)

```bash
clang-tidy --config-file=.clang-tidy src/main.cpp --
```

Requires `metis init --enable-clang-tidy` to have created `.clang-tidy`.

---

### CMake Scaffolding

`metis init --enable-cmake` generates a production-grade `CMakeLists.txt` and a minimal `src/main.cpp`:

```bash
metis init --enable-cmake
```

Creates:
- `CMakeLists.txt`: full CMake project configuration
- `src/main.cpp`: entry point with `main()` stub

**CMake Options**

| Flag | Values | Default | Description |
|------|--------|---------|-------------|
| `--cmake-cpp-standard` | `17`, `20`, `23` | `20` | C++ standard version |
| `--cmake-target-type` | `executable`, `static`, `shared`, `header-only` | `executable` | Build target type |
| `--cmake-enable-testing` | (flag) | off | Add `enable_testing()` + `add_subdirectory(tests)` |
| `--cmake-enable-sanitizers` | (flag) | off | AddressSanitizer + UBSan (Debug only) |
| `--enable-conan` | (flag) | off | Generate `conanfile.py` alongside CMakeLists.txt for Conan package manager |

**What the generated CMakeLists.txt includes:**

- `cmake_minimum_required(VERSION 3.20)` with project declaration
- C++ standard enforcement (`CMAKE_CXX_STANDARD_REQUIRED ON`, extensions OFF)
- Build type configuration (Debug + Release)
- `CMAKE_EXPORT_COMPILE_COMMANDS` for IDE support
- Source file and include directory setup
- Compiler warnings: `-Wall -Wextra -Wpedantic -Wconversion -Wshadow` (GCC/Clang) or `/W4` (MSVC)
- Sanitizers: address + undefined behaviour in Debug builds
- **clang-tidy integration** (if `--enable-clang-tidy` was also passed)
- **clang-format** custom target (`make format`)
- Testing: `enable_testing() + add_subdirectory(tests)` (if `--cmake-enable-testing`)
- Installation rules with GNUInstallDirs

**Examples**

```bash
# Minimal C++20 executable project
metis init --enable-cmake

# C++17 static library with testing
metis init --enable-cmake --cmake-cpp-standard 17 --cmake-target-type static --cmake-enable-testing

# Full-featured project with clang-tidy + sanitizers
metis init \
  --enable-cmake \
  --cmake-cpp-standard 20 \
  --cmake-enable-testing \
  --cmake-enable-sanitizers \
  --enable-clang-tidy \
  --tidy-preset strict \
  --style google

# Header-only library (no src/main.cpp generated)
metis init --enable-cmake --cmake-target-type header-only

# Project with Conan package manager support
metis init --enable-cmake --enable-conan

# Full-featured project with Conan + clang-tidy + testing
metis init \
  --enable-cmake \
  --enable-conan \
  --cmake-cpp-standard 20 \
  --cmake-enable-testing \
  --enable-clang-tidy \
  --tidy-preset strict \
  --style google
```

**Generate `src/main.cpp` without CMake:**

```bash
metis init --generate-src
```

Creates only `src/main.cpp`, no `CMakeLists.txt`.

---

### Conan Package Manager Integration

When `--enable-conan` is passed with `--enable-cmake`, metis generates:

- `conanfile.py`: Conan package definition with CMakeToolchain/CMakeDeps
- `CMakeLists.txt`: Updated to use `find_package()` instead of FetchContent

The generated `conanfile.py` includes:
- Python class-based Conan recipe
- CMakeToolchain and CMakeDeps generators
- Proper dependency resolution from `.metis.toml`

**What the generated CMakeLists.txt includes (with Conan):**

- Uses `find_package(fmt REQUIRED)` instead of FetchContent
- Uses `find_package(tomlplusplus REQUIRED)` instead of FetchContent
- Links against imported targets (`fmt::fmt`, `tomlplusplus::tomlplusplus`)

**Usage example:**

```bash
# Generate with Conan support
metis init --enable-cmake --enable-conan

# Build with Conan
conan install . --output-folder=build --build=missing
cmake -B build -DCMAKE_TOOLCHAIN_FILE=build/conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## Troubleshooting

| Symptom | Likely cause | Fix |
|---------|-------------|-----|
| `clang-tidy: cannot find ...` | `.clang-tidy` missing | `metis init --enable-clang-tidy` |
| `bash: pre-commit: No such file or directory` | Hook not installed | `metis install` |
| Hook exits immediately with no output | File patterns don't match staged files | Run `metis run --all-files` to test outside hook |
| `git commit` but hook doesn't run | Staged files don't match config checks | Check `patterns` in the `[[checks]]` sections of `.metis.toml` |
| CI workflow not created | `github_actions` is `false` in config | `metis generate-gha` (bypasses config) |
| GitLab CI not created | `gitlab_ci` is `false` in config | `metis generate-gitlab` (bypasses config) |

**Debug mode**: Run `metis run --verbose --all-files` to see the exact shell commands being executed.

## Acknowledgements

- [pre-commit](https://pre-commit.com/): inspiration for hook management workflow

This project was inspired by an internal tool developed at a previous company for enforcing C/C++ naming conventions. Due to licensing constraints, metis was built from scratch with the same functionality but a distinct architecture and open-source ethos.
