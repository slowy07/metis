## v0.5.0: Result Cache, Perf/Deps/Build Subcommands, Per-Command Help

### What's Changed

- **Result cache for `metis run`** -- successful check results are now cached in `.metis-cache/`, keyed by a hash of the check command/args plus per-file size and mtime fingerprints. Unchanged checks on unchanged files are skipped on repeat runs.
- **`--diff-only`** -- check just the diff (staged files) with the cache enabled; the default `metis run` already targets staged files. `--all-files` sweeps the whole tracked codebase and never uses the cache. `--no-cache` bypasses caching on a diff run.
- **Cache use-after-free fix** -- the cache previously freed its backing storage before checks executed, so no cache entry was ever written; it now lives for the full run and the cache works correctly.
- **`perf` subcommand** -- performance checks for build time, binary size, and benchmarks via `PerfChecksUseCase`, configured in the `[perf]` TOML section. `--level quick` runs binary size only; `--level full` (default) also covers build time and benchmarks.
- **`deps` subcommand** -- validates project dependencies across Conan, vcpkg, and CMake FetchContent via `DependencyCheckUseCase`, with duplicate detection and lockfile awareness. `--tree` displays a dependency tree; `--graph` generates a graph.
- **`build` subcommand** -- configure and build the project with CMake via `BuildUseCase`. Supports `--build-dir`, `--clean`, `--verbose`, and `-j/--jobs`.
- **Per-subcommand `--help`** -- every subcommand (`init`, `install`, `sync`, `run`, `test`, `sanitizer`, `deps`, `build`, `perf`, `install-compiler`, `generate-gha`, `generate-gitlab`) now shows its own detailed usage via `metis <command> --help`.
- **`sync` subcommand** -- refreshes hooks/workflows, generates missing tool configs, and validates all configured checks.
- **Interactive init improvements** -- richer prompting and detail information in the init wizard.
- **New unit tests** -- argparse parsing, security checks, dependency checks, perf checks, and the check cache (store/lookup, arg mismatch, changed-file invalidation).
- **Docs overhaul** -- usage, architecture, dependency, and formatting docs refreshed for the new subcommands, flags, and cache behavior.

### Usage: `metis run` Modes

- `--diff-only` -- check only the diff (staged files), cache enabled.
- `--all-files` -- check every tracked file in the repo, cache disabled.
- `<files...>` -- check specific files explicitly.
- `--no-cache` -- disable the result cache for a diff run.
- `--detail` -- verbose output showing pass/fail per check.

### Install

**Linux:**
```sh
curl -LsSf https://raw.githubusercontent.com/slowy07/metis/main/install.sh | sh
```

**Windows (Powershell):**
```powershell
powershell -ExecutionPolicy ByPass -c "irm https://raw.githubusercontent.com/slowy07/metis/main/install.ps1 | iex"
```

**Arch Linux (AUR):**
```sh
yay -S metis
```
