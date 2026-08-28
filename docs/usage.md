# Usage

metis is a single static binary that runs your project's checks before
commit. It reads a TOML config, installs a `.git/hooks/pre-commit` script, and
can also run checks directly.

## Quick start

```bash
metis init --enable-clang-tidy   # writes .metis.toml, .clang-format, .clang-tidy
metis install                    # writes .git/hooks/pre-commit
git add . && git commit                  # hook fires automatically
metis run --all-files            # run checks ad-hoc, no commit
```

Exit code `0` = all checks passed, non-zero = at least one failed.

## Subcommands

| Command | Action |
|---|---|
| `init` | Generate `.metis.toml` + tooling configs |
| `install` | Install `.git/hooks/pre-commit` (and CI workflows if enabled in config) |
| `sync` | Refresh hooks/workflows, generate missing tool configs, validate all checks |
| `run` | Execute configured checks on files |
| `generate-gha` | Write `.github/workflows/metis.yml` |
| `generate-gitlab` | Write `.gitlab-ci.yml` |
| `install-compiler` | Download and install a C++ toolchain |
| `test` | Run ctest and optional coverage checks |
| `sanitizer` | Run sanitizer checks (ASan, UBSan, TSan, LSan) |
| `deps` | Check project dependencies (Conan, vcpkg, CMake FetchContent) |
| `build` | Configure and build the project with CMake |
| `perf` | Run performance checks (build time, binary size, benchmarks); `--level quick` = binary size only; default: `full` |
| `-h, --help` / `-v, --version` | Help / version |

Global flag: `-c, --config <path>` (default `.metis.toml`). Every subcommand
accepts `--help` to show its own detailed usage, e.g. `metis run --help`.

## init

With no flags, `init` starts an interactive wizard. Any flag disables it
(`--interactive`/`-i` forces it back on).

```bash
metis init --style google --enable-clang-tidy
```

| Flag | Values | Default |
|---|---|---|
| `--style` | `google`, `llvm`, `chromium`, `mozilla`, `webkit`, `microsoft`, `gnu` | `Google` |
| `--name` | project name | directory name |
| `--indent-width` | int | `2` |
| `--column-limit` | int | `100` |
| `--pointer-alignment` | `Left`, `Right`, `Middle` | `Left` |
| `--brace-style` | e.g. `Attach` | `Attach` |
| `--enable-clang-tidy`, `--tidy` | flag | off |
| `--tidy-preset` | `minimal`, `standard`, `strict`, `custom` | `standard` |
| `--tidy-severity` | `note`, `warning`, `error` | `error` |
| `--tidy-header-filter` | `0` none, `1` project, `2` all | `1` |
| `--enable-cmake`, `--cmake` | flag | off |
| `--cmake-cpp-standard` | `17`, `20`, `23` | `20` |
| `--cmake-target-type` | `executable`, `static`, `shared`, `header-only` | `executable` |
| `--cmake-enable-testing` | flag | off |
| `--cmake-enable-sanitizers` | flag | off |
| `--enable-conan` | flag | off |
| `--add-dep` | package name (repeatable) | - |
| `--generate-src` | flag | off |
| `--enable-compiler-checks`, `--compiler-checks` | flag | off |
| `--compiler` | `g++`, `clang++`, `gcc`, `clang` | `g++` |
| `--compiler-cpp-standard` | `17`, `20`, `23`, `26` | `20` |
| `--compiler-werror` / `--compiler-no-werror` | flag | on |
| `--compiler-debug-and-release` | flag | off |
| `--enable-security-checks`, `--security-checks` | flag | off |

What `init` creates: `.metis.toml` always; `.clang-format` always;
`.clang-tidy` with `--enable-clang-tidy`; `CMakeLists.txt` + `src/main.cpp`
with `--enable-cmake`; `conanfile.py` with `--enable-conan`.
With `--enable-compiler-checks`, `.metis.toml` gains `[[checks]]`
entries that syntax-check each file with your compiler (`g++ -std=c++20 -Wall
-Wextra -Wpedantic [-Werror] -fsyntax-only ...`). `--compiler-debug-and-release`
emits two checks (`-O0 -g -D_DEBUG` / `-O2 -DNDEBUG`) instead of one.
With `--enable-security-checks`, `.metis.toml` gains `metis-security` (hardcoded
secrets / dangerous functions) and `metis-dep-security` (dependency vulnerability
scan) checks.

## run

```bash
metis run                     # staged files (default, cached)
metis run --diff-only         # check just the diff (staged files, cached)
metis run --all-files         # all tracked files (no cache)
metis run src/a.cpp include/b.hpp  # explicit files
metis run --dry-run --all-files    # list files, don't execute
metis run --verbose --all-files    # echo each shell command
metis run --format            # run clang-format -i in-place
metis run --no-cache          # bypass the check result cache
```

The cache only helps on a diff: successful staged/explicit-file check
results are cached in `.metis-cache/` and reused when the check config and
inputs are unchanged, so unchanged files are skipped on repeat runs.
`--diff-only` runs just the diff (staged files) with the cache enabled. A
full-codebase sweep (`--all-files`) always re-checks everything and never
uses the cache. Pass `--no-cache` to disable caching on a diff run.

## Config file

`.metis.toml` drives every check:

```toml
[project]
name = "my-project"

[[checks]]
name = "clang-format"
command = "clang-format"
args = ["-i", "--fallback-style=google", "-style=file"]
patterns = ["*.cpp", "*.hpp", "*.h", "*.cc"]

[exclude]
paths = ["build/", "third_party/", ".git/"]

[output]
local_hook = true        # install the git hook
github_actions = false   # also write .github/workflows on `install`

[execution]
parallel = true
```

- `[[checks]]`: `name` (unique), `description`, `enabled` (`false` skips the
  check), `command`, `args`, `patterns` (glob matched against file paths),
  `timeout` (seconds), `severity` (`error`/`warning`/`info`). Checks run in
  parallel; output is serialized.
- `[exclude]`: `paths` are matched as prefix, exact, or `*.ext` glob.
- `generate-gha` / `generate-gitlab` write workflows regardless of `[output]`.
- grep/rg checks: exit code 1 (no match) counts as pass, so a
  `grep -E "[[:space:]]+$"` trailing-whitespace check fails only on a match.

## Test and sanitizer config

```toml
[test]
build_dir = "build"
coverage = false
line_threshold = 80.0
branch_threshold = 70.0
function_threshold = 90.0

[sanitizers]
enabled = false
types = ["address", "undefined"]
build_dir = "build"
timeout = 0
```

`[test]`: `build_dir` is the CMake build directory for `ctest`. `coverage`
enables coverage reporting. Thresholds are minimum percentages; the check
fails if any metric falls below.

`[sanitizers]`: `types` accepts `address`, `undefined`, `thread`, `leak`.
`build_dir` is where the sanitizer build is created. `timeout` is max seconds
per test (`0` = no limit).

# Perf config

```toml
[perf]
enabled = false
build_dir = "build"
binary_path = ""
max_binary_size_mb = 0
max_build_time_sec = 0
benchmark_regex = ""
```

`[perf]`: each check activates only when its threshold/target is set.
`max_build_time_sec > 0` re-builds `build_dir` with `--clean-first` and fails
if the measured wall time exceeds it. `binary_path` + `max_binary_size_mb > 0`
fails if the binary exceeds the size. `benchmark_regex` runs
`ctest --test-dir <build_dir> -R "<regex>" --output-on-failure`. Exit codes:
`21` binary too large, `22` build too slow, `20` other perf failure.

## Check types

`[[checks]]` is generic: the behavior is selected **automatically by `command`
basename**, so the same config surface covers every tool. Unknown commands run
as a custom shell check.

| `command` basename | Behavior | Files |
|---|---|---|
| `clang-format` | In-place formatting; reports `Formatted`/`Clean` per file; exit `0` after applying fixes | once (batched), C/C++ only |
| `clang-tidy` | Static analysis; requires `.clang-tidy` or `--config-file=` | once (batched) |
| `gcc`, `g++`, `clang`, `clang++`, `cc`, `c++` (or versioned: `gcc-14`, `clang++-17`) | Syntax-only compile; `-fsyntax-only` is forced unless `args` already set `-c`/`-S`/`-E` | per-file |
| `cppcheck` | Static analysis via cppcheck; batches all files | once (batched) |
| `gcc-analyzer` | GCC static analyzer (`-fanalyzer`); per-file | per-file |
| `clang-static-analyzer` | Clang static analyzer; per-file | per-file |
| `include-what-you-use`, `iwyu` | Include-what-you-use analysis; per-file | per-file |
| `cmake` | Build command (e.g. `cmake --build build`); file list omitted | once |
| `git` | e.g. `git diff --check`; runs against the whole working tree; file list omitted | once |
| `metis-security` | Scans files for hardcoded secrets (`password=`, `api_key=`, tokens, ...); match fails the check | per-file |
| `metis-dep-security` | Vulnerability scan of dependencies via `osv-scanner` (falls back to `grype`); needs either installed | once |
| anything else | Custom shell command; file list appended | per-file (batched for `clang-format`, `clang-tidy`, `grep`, `egrep`, `rg`, `cppcheck`) |

Notes:

- **grep/rg exit codes are inverted**: exit `0` (match found) fails the check,
  exit `1` (no match) passes.
- **clang-format** always runs with `-i` (a user-supplied `-i` is stripped) and
  needs `.clang-format` or `_clang-format`: otherwise the check fails before
  anything runs. Stage the fixes with `git add -u`.
- **clang-tidy** fails before execution if `.clang-tidy` is missing, unless
  `args` includes `--config-file=<path>`. A bare `--` in `args` separates
  tidy options from compiler flags (e.g.
  `args = ["--quiet", "--", "-std=c++20", "-Iinclude"]`); source files are
  inserted between them. Without a `--`, files are appended after the options.
- **compiler checks** compile each file with `-fsyntax-only`, so no object
  files ever land in the repo.
- Dispatch matches `clang-format`/`clang-tidy`/`cmake`/`git` by **exact
  basename**; `clang-format-17` falls back to a custom shell check. Compilers
  are the exception: `gcc-14` still routes to the compiler check.
- `timeout` is validated but not yet enforced; `severity`
  (`error`/`warning`/`info`) is stored but does not yet change the exit code.
