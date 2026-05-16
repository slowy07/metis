# sniffercommit

Fast, C++20-powered pre-commit hook and CI generator. Ensures code quality before every push.

## Features

- **Parallel check execution** — run linters concurrently via Bash background jobs
- **Pattern-aware filtering** — apply checks only to matching file extensions (`.cpp`, `.hpp`, etc.)
- **Zero runtime dependencies** — single static binary; no Python/Node required
- **Auto-generate CI workflow** — GitHub Actions workflow mirroring local hooks
- **.clang-format scaffolding** — generate `.clang-format` with configurable style presets

## Dependencies

| Dependency | Purpose |
|------------|---------|
| [`tomlplusplus`](https://github.com/marzer/tomlplusplus.git) | Header-only TOML config file parser and serializer |
| [`fmt`](https://github.com/fmtlib/fmt.git) | Modern formatting library |

Both are fetched automatically at configure time via CMake's `FetchContent`. No system installation required.

## Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel

# verify
./build/sniffercommit --version
```

## Usage

### Initialize a project

```bash
cd /path/to/project
sniffercommit init

# optional: specify formatter style
# available: google, llvm, chromium, mozilla, webkit, microsoft, gnu
sniffercommit init --style llvm

# optional: custom project name, indent width, column limit, etc.
sniffercommit init --style google --name my-project --indent-width 4 --column-limit 120
```

This creates two files:
- **`.sniffercommit.toml`** — check configuration with sensible defaults
- **`.clang-format`** — formatter style matching your chosen preset

### Install pre-commit hooks

```bash
sniffercommit install
```

Generates and installs `.git/hooks/pre-commit` (and optionally `.github/workflows/sniffercommit.yml`).

### Generate CI workflow only

```bash
sniffercommit generate-gha
```

### Run checks manually

```bash
# all tracked files
sniffercommit run --all-files

# specific files
sniffercommit run src/main.cpp

# dry-run
sniffercommit run --dry-run --all-files
```

### Test the hook

```bash
echo "int main(){return 0;}" > test.cpp
git add test.cpp
git commit -m "chore: test sniffercommit"
```

## Init Options

| Flag | Description |
|------|-------------|
| `--style <name>` | `google`, `llvm`, `chromium`, `mozilla`, `webkit`, `microsoft`, `gnu` (default: `google`) |
| `--name <name>` | Project name (default: current directory name) |
| `--indent-width <n>` | Indentation width (default: `2`) |
| `--column-limit <n>` | Column limit (default: `100`) |
| `--pointer-alignment <s>` | Pointer alignment: `Left`, `Right`, `Middle` |
| `--brace-style <s>` | Brace placement: `Attach`, `Allman`, etc. |

## Debugging

```bash
# enable verbose config output
cmake -B build -DSNIFFERCOMMIT_VERBOSE_CONFIG=ON

# build with sanitizers (catch memory bugs)
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DSNIFFERCOMMIT_ENABLE_SANITIZERS=ON

# inspect generated hook
cat .git/hooks/pre-commit | less

# test hook manually (without git)
bash .git/hooks/pre-commit
```

## Acknowledgements

- [pre-commit](https://pre-commit.com/) — inspiration for hook management workflow

This project was inspired by an internal tool developed at a previous company for enforcing C/C++ naming conventions. Due to licensing constraints, sniffercommit was built from scratch with the same functionality but a distinct architecture and open-source ethos.
