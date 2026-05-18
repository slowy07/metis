# sniffercommit

Fast, C++20-powered pre-commit hook and CI generator. Ensures code quality before every push with zero runtime dependencies — a single static binary that replaces Python- and Node-based pre-commit frameworks.

## Features

- **Parallel check execution** : run linters concurrently via Bash background jobs
- **Pattern-aware filtering** : apply checks only to matching file extensions (`.cpp`, `.hpp`, etc.)
- **Zero runtime dependencies** : single static binary; no Python/Node required
- **Auto-generate CI workflow** : GitHub Actions workflow mirroring local hooks
- **.clang-format scaffolding** : generate `.clang-format` with configurable style presets
- **.clang-tidy** integration : static analysis with curated check preset (`minimal`, `standard`, `strict`)
- **Hook syntax validation** : generated bash hooks are validated with `bash -n` before install
- **Git worktree support** : works in worktrees, submodules, and detached checkouts

## Dependencies

| Dependency | Purpose | Version |
| ------------- | -------------- | -------------- |
| [tomlplusplus](https://github.com/marzer/tomlplusplus.git) | Header-only TOML config file parser and serializer | `v3.4.0` |
| [fmt](https://github.com/fmtlib/fmt.git) | Modern formatting library | `v11.0.2` |

Both are fetched automatically at configure time via CMake's `FetchContent`. No system installation required.
Optional system packages (if `SNIFFERCOMMIT_USE_SYSTEM_FMT=ON`):

```bash
# Ubuntu / Debian
sudo apt-get install libfmt-dev

# macOS (Homebrew)
brew install fmt

# Arch
sudo pacman -S fmt
```

## Build

```
# Release build (recommended)
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel

# Verify
./build/sniffercommit --version
```

| Option                                  | Default | Description                                       |
| --------------------------------------- | ------- | ------------------------------------------------- |
| `SNIFFERCOMMIT_ENABLE_SANITIZERS`       | `OFF`   | AddressSanitizer + UBSan (Debug only)             |
| `SNIFFERCOMMIT_ENABLE_STATIC_LINK`      | `OFF`   | Fully portable static binary (Linux only)         |
| `SNIFFERCOMMIT_VERBOSE_CONFIG`          | `ON`    | Print platform/compiler summary at configure      |
| `SNIFFERCOMMIT_USE_SYSTEM_FMT`          | `ON`    | Use system `libfmt` instead of FetchContent       |
| `SNIFFERCOMMIT_USE_SYSTEM_TOMLPLUSPLUS` | `OFF`   | Use system `tomlplusplus` instead of FetchContent |

**Compiler Requirements**

- `GCC >= 11`
- `Clang >= 14`
- `MSVC >= 2022 17.0`

```bash
# System-wide install
sudo cmake --install build

# Or create Debian package
cd build && cpack -G DEB
sudo dpkg -i sniffercommit_*.deb

# Install via pacman (AUR)
yay -S sniffercommit
```

## Usage

**Initialize Project**

```bash
cd /path/to/projects
sniffercommit init
```
This creates:
    - `.sniffercommit.toml` : check configuration with sensible defaults
    - `.clang-format` : formatter style matching your chosen preset

**With clang-tidy (static analysis)**
```bash
# enable clang-tidy with the standard preset
sniffercommit init --enable-clang-tidy

# use strict preset (all checks minus noisy ones)
sniffercommit init --enable-clang-tidy --tidy-preset strict

# custom severity
sniffercommit init --enable-clang-tidy --tidy-severity warning
```
When `--enable-clang-tidy` is used, three files are created:
    - `.sniffercommit.toml` : includes `clang-tidy` check
    - `.clang-format` : formatter config
    - `.clang-tidy` : static analysis config with curated check preset

**Formatter Style Options**
```bash
# available styles: google, llvm, chromium, mozilla, webkit, microsoft, gnu
sniffercommit init --style llvm

# full customization
sniffercommit init \
  --style google \
  --name my-project \
  --indent-width 4 \
  --column-limit 120 \
  --pointer-alignment Left \
  --brace-style Attach
```

**Install pre-commit hooks**
```bash
sniffercommit install
```
Generates and installs:
    - `.git/hooks/pre-commit` : bash hook with parallel execution
    - `.github/workflows/sniffercommit.yml` : GitHub Actions workflow (if `github_actions = true` in config)

The hook is validated with `bash -n` before installation to prevent broken hooks.

**Uninstall hooks**:
```bash
# remove the pre-commit hooks
rm .git/hooks/pre-commit

# or regenerate from config
sniffercommit install
```

**Generate CI workflow only**
```bash
sniffercommit generate-gha
```

**Run check manually**
```bash
# all tracked files
sniffercommit run --all-files

# staged files only (default)
sniffercommit run

# specific files
sniffercommit run src/main.cpp include/foo.hpp

# dry-run (list files without executing)
sniffercommit run --dry-run --all-files

# verbose output (print each command)
sniffercommit run --verbose --all-files
```

## Acknowledgements

- [pre-commit](https://pre-commit.com/) — inspiration for hook management workflow

This project was inspired by an internal tool developed at a previous company for enforcing C/C++ naming conventions. Due to licensing constraints, sniffercommit was built from scratch with the same functionality but a distinct architecture and open-source ethos.
