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

## Quick Start

```bash
cd my-project

# Initialize config and tooling files
sniffercommit init --style google --enable-clang-tidy

# Install the pre-commit hook (auto-runs on every `git commit`)
sniffercommit install

# Stage and commit — hook fires automatically
git add .
git commit -m "initial"

# Or run checks ad-hoc without committing
sniffercommit run --all-files
```

`exit code 0` → all checks pass. Non-zero → at least one check failed.

## Subcommands

| Command | Action |
|---------|--------|
| `init` | Generate `.sniffercommit.toml`, `.clang-format` (and `.clang-tidy` with `--enable-clang-tidy`) |
| `install` | Install pre-commit hook + optional CI workflow |
| `run` | Execute checks on files (staged by default, `--all-files` for tracked) |
| `generate-gha` | Write GitHub Actions workflow to `.github/workflows/sniffercommit.yml` |
| `--version`, `-v` | Print version and exit |
| `--help` | Print help and exit |

Global flag: `--config <path>` — use a non-default config file path with any subcommand.

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

```bash
# Release build (recommended)
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel

# Verify
./build/sniffercommit --version

# Quick smoke test
./build/sniffercommit init --style google --enable-clang-tidy
./build/sniffercommit run --dry-run --all-files
./build/sniffercommit install
```

| Option | Default | Description |
| ----- | ------- | ----------- |
| `SNIFFERCOMMIT_ENABLE_SANITIZERS` | `OFF` | AddressSanitizer + UBSan (Debug only) |
| `SNIFFERCOMMIT_ENABLE_STATIC_LINK` | `OFF` | Fully portable static binary (Linux only) |
| `SNIFFERCOMMIT_VERBOSE_CONFIG` | `ON` | Print platform/compiler summary at configure |
| `SNIFFERCOMMIT_USE_SYSTEM_FMT` | `ON` | Use system `libfmt` instead of FetchContent |
| `SNIFFERCOMMIT_USE_SYSTEM_TOMLPLUSPLUS` | `OFF` | Use system `tomlplusplus` instead of FetchContent |

**Compiler Requirements**

- `GCC >= 11`
- `Clang >= 14`
- `MSVC >= 2022 17.0`

**Install**

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

### init — Generate Configuration

```bash
cd /path/to/project
sniffercommit init
```

Creates:
- `.sniffercommit.toml` — check configuration with sensible defaults
- `.clang-format` — formatter style matching your chosen preset

**With clang-tidy (static analysis)**

```bash
sniffercommit init --enable-clang-tidy                        # standard preset
sniffercommit init --enable-clang-tidy --tidy-preset strict   # strict preset
sniffercommit init --enable-clang-tidy --tidy-severity warning # compiler warnings only
```

With `--enable-clang-tidy`, three files are created:
- `.sniffercommit.toml` — includes `clang-tidy` check
- `.clang-format` — formatter config
- `.clang-tidy` — static analysis config with curated check preset

**Formatter & Analyzer Options**

```bash
# available styles: google, llvm, chromium, mozilla, webkit, microsoft, gnu
sniffercommit init --style llvm

# clang-tidy preset: minimal, standard (default), strict, custom
sniffercommit init --enable-clang-tidy --tidy-preset standard

# tidy severity: note (off), warning (compiler only), error (all)
sniffercommit init --enable-clang-tidy --tidy-severity warning

# header filter: 0=none, 1=project, 2=all
sniffercommit init --enable-clang-tidy --tidy-header-filter 1

# enable cmake and generate src file
# src/main.cpp
sniffercommit init --enable-cmake

# full customization
sniffercommit init \
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
sniffercommit --config /path/to/.sniffercommit.toml init
```

---

### install — Install Pre-commit Hook

```bash
sniffercommit install
```

Installs:
- `.git/hooks/pre-commit` — Bash hook with parallel execution
- `.github/workflows/sniffercommit.yml` — GitHub Actions workflow (only if `github_actions = true` in config)

The hook is validated with `bash -n` before installation to prevent broken hooks.

**Uninstall**

```bash
rm .git/hooks/pre-commit
rm -rf .github/workflows/sniffercommit.yml
```

Or re-run `sniffercommit install` to regenerate both files from config.

---

### run — Execute Checks Manually

```bash
# staged files only (default)
sniffercommit run

# all tracked files
sniffercommit run --all-files

# specific files
sniffercommit run src/main.cpp include/foo.hpp

# dry-run (list files without executing)
sniffercommit run --dry-run --all-files

# verbose (print each shell command)
sniffercommit run --verbose --all-files
```

Exit code: `0` if all checks pass, non-zero if any check fails.

---

### generate-gha — CI Workflow Only

```bash
sniffercommit generate-gha
```

Always writes `.github/workflows/sniffercommit.yml` regardless of the `github_actions` config setting.

---

### Tidy Presets

Used with `sniffercommit init --enable-clang-tidy`:

| Preset | Scope |
|--------|-------|
| `minimal` | cppcoreguidelines + bugprone + clang-analyzer |
| `standard` (default) | minimal + modernize + performance |
| `strict` | All checks minus noisy ones (abseil, altera, fuchsia, llvm, zircon) |
| `custom` | User-defined via `extra_checks` / `exclude_checks` |

---

### Manual clang-tidy (no sniffercommit wrapper)

```bash
clang-tidy --config-file=.clang-tidy src/main.cpp --
```

Requires `sniffercommit init --enable-clang-tidy` to have created `.clang-tidy`.

## Troubleshooting

| Symptom | Likely cause | Fix |
|---------|-------------|-----|
| `clang-tidy: cannot find ...` | `.clang-tidy` missing | `sniffercommit init --enable-clang-tidy` |
| `bash: pre-commit: No such file or directory` | Hook not installed | `sniffercommit install` |
| Hook exits immediately with no output | File patterns don't match staged files | Run `sniffercommit run --all-files` to test outside hook |
| `git commit` but hook doesn't run | Staged files don't match config checks | Check `[checks.*.files]` patterns in `.sniffercommit.toml` |
| CI workflow not created | `github_actions` is `false` in config | `sniffercommit generate-gha` (bypasses config) |

**Debug mode**: Run `sniffercommit run --verbose --all-files` to see the exact shell commands being executed.

## Acknowledgements

- [pre-commit](https://pre-commit.com/) — inspiration for hook management workflow

This project was inspired by an internal tool developed at a previous company for enforcing C/C++ naming conventions. Due to licensing constraints, sniffercommit was built from scratch with the same functionality but a distinct architecture and open-source ethos.
