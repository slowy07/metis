## v0.4.0: New Check Types, Test & Sanitizer Subcommands, SHA-256 Verification

### What's Changed

- **New check types** -- `cppcheck`, `gcc-analyzer`, `clang-static-analyzer`, `iwyu` (include-what-you-use) are now recognized by the `make_check` factory and run with proper defaults.
- **`test` subcommand** -- runs `ctest` against a build directory with optional `--coverage` reporting and line/branch/function threshold enforcement via `TestChecksUseCase`.
- **`sanitizer` subcommand** -- builds and tests with ASan/UBSan/TSan/LSan via `SanitizerChecksUseCase`, reading types from the `[sanitizers]` TOML section.
- **`generate_sanitizer_config()`** -- new config generator that emits a `[sanitizer]` section for `.metis.toml`.
- **SHA-256 verification** -- Windows GCC installer now verifies downloaded archives against a SHA-256 checksum, with fail-closed behavior on fetch failure.
- **Generic check abstraction** -- `domain::check::Check` base class with `validate()`/`execute()` contract; concrete checks dispatch by command basename via `make_check` factory.
- **Compiler check fixes** -- mode detection (`-c`/`-S`/`-E`) prevents redundant `-fsyntax-only` injection; `--compiler-debug-and-release` emits two checks instead of one.
- **Interactive init fixes** -- `std::to_array` compile errors resolved; compiler checks config append moved before file write; default `compiler_debug_and_release` wired correctly.
- **`show_help()` updated** -- all 7 subcommands now have `Usage:` examples in `--help` output.
- **17 new unit tests** -- config generators, config validation, sanitizer checks, SHA-256 verification, TestChecksUseCase.
- **Docs overhaul** -- exit codes table expanded to 20 entries; `install-gcc` renamed to `install-compiler` throughout; testing section rewritten for GoogleTest + CTest.

### Usage: `metis run` Modes

- `--all-files` -- check every tracked file in the repo.
- `--staged` -- check only git-staged files (default for pre-commit hooks).
- `<files...>` -- check specific files explicitly.
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
