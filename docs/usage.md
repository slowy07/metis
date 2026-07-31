# Usage

sniffercommit is a single static binary that runs your project's checks before
commit. It reads a TOML config, installs a `.git/hooks/pre-commit` script, and
can also run checks directly.

## Quick start

```bash
sniffercommit init --enable-clang-tidy   # writes .sniffercommit.toml, .clang-format, .clang-tidy
sniffercommit install                    # writes .git/hooks/pre-commit
git add . && git commit                  # hook fires automatically
sniffercommit run --all-files            # run checks ad-hoc, no commit
```

Exit code `0` = all checks passed, non-zero = at least one failed.

## Subcommands

| Command | Action |
|---|---|
| `init` | Generate `.sniffercommit.toml` + tooling configs |
| `install` | Install `.git/hooks/pre-commit` (and CI workflows if enabled in config) |
| `run` | Execute configured checks on files |
| `generate-gha` | Write `.github/workflows/sniffercommit.yml` |
| `generate-gitlab` | Write `.gitlab-ci.yml` |
| `-h, --help` / `-v, --version` | Help / version |

Global flag: `-c, --config <path>` (default `.sniffercommit.toml`).

## init

With no flags, `init` starts an interactive wizard. Any flag disables it
(`--interactive`/`-i` forces it back on).

```bash
sniffercommit init --style google --enable-clang-tidy
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
| `--add-dep` | package name (repeatable) | — |
| `--generate-src` | flag | off |

What `init` creates: `.sniffercommit.toml` always; `.clang-format` always;
`.clang-tidy` with `--enable-clang-tidy`; `CMakeLists.txt` + `src/main.cpp`
with `--enable-cmake`; `conanfile.py` with `--enable-conan`.

## run

```bash
sniffercommit run                     # staged files (default)
sniffercommit run --all-files         # all tracked files
sniffercommit run src/a.cpp include/b.hpp  # explicit files
sniffercommit run --dry-run --all-files    # list files, don't execute
sniffercommit run --verbose --all-files    # echo each shell command
sniffercommit run --format            # run clang-format -i in-place
```

## Config file

`.sniffercommit.toml` drives every check:

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

- `[[checks]]` — `name` (unique), `command`, `args`, `patterns` (glob matched
  against file paths). Checks run in parallel; output is serialized.
- `[exclude]` — `paths` are matched as prefix, exact, or `*.ext` glob.
- `generate-gha` / `generate-gitlab` write workflows regardless of `[output]`.
- grep/rg checks: exit code 1 (no match) counts as pass, so a
  `grep -E "[[:space:]]+$"` trailing-whitespace check fails only on a match.
