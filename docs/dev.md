# Development

## Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DSNIFFERCOMMIT_ENABLE_SANITIZERS=ON
cmake --build build --parallel
```

Binary lands at `build/bin/sniffercommit`; tests at `build/bin/sniffercommit_tests`.

| Option | Default | Purpose |
|---|---|---|
| `SNIFFERCOMMIT_BUILD_TESTS` | `ON` | Build GoogleTest suite |
| `SNIFFERCOMMIT_ENABLE_SANITIZERS` | `OFF` | ASan + UBSan (Debug only) |
| `SNIFFERCOMMIT_USE_SYSTEM_FMT` | `OFF` | Use system fmt instead of FetchContent |
| `SNIFFERCOMMIT_USE_SYSTEM_TOMLPLUSPLUS` | `OFF` | Use system tomlplusplus instead of FetchContent |

## Tests

```bash
ctest --test-dir build --output-on-failure
```

Tests live in `tests/` (GoogleTest). Conventions: `UnitUnderTest_Scenario_ExpectedResult`
naming, temp dirs in fixtures for git/fs-dependent tests.

## Lint

The project lints itself — `install.sh`-installed or built binary:

```bash
build/bin/sniffercommit init --style google --enable-clang-tidy
build/bin/sniffercommit install
```

The pre-commit hook runs clang-format, clang-tidy, and trailing-whitespace.
`tests/.clang-tidy` overrides the root config for test sources (more permissive).

## Layout

```
src/
  main.cpp                    CLI entry, arg parsing, DI wiring
  application/                use cases: init, install, run, install-toolchain, generate-workflow
  domain/                     config, workflow models + defaults, exit codes
  domain/ports/               interfaces: shell, fs, git, config, http, archive, toolchain
  generators/                 clang-format / clang-tidy / cmake / conan templates
  infrastructure/             concrete adapters: toml, shell, fs, git, curl, tar, toolchain providers
  presentation/               interactive init wizard, output helpers
include/sniffercommit/        public headers, mirrored under src/
```

Data flow: `main.cpp` parses args → builds adapters → injects into a use case
→ use case reads `domain` models and drives `infrastructure` through the
`ports` interfaces.

## Conventions

- C++20, 2-space indent, 100-col limit, attach braces (`.clang-format`).
- `snake_case` functions/vars, `PascalCase` types, `kConstant` constants,
  trailing `_` on members.
- Dependencies: only `fmt` and `tomlplusplus`, fetched via FetchContent.
- Commit messages: Conventional Commits (`feat(scope): ...`).

## Adding a check

Checks are config-driven, not code — a user adds one via `[[checks]]` in
`.sniffercommit.toml`. Code changes are only needed if a check needs new
runtime behavior (e.g. batch mode, exit-code inversion).
