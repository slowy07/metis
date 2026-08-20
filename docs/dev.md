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

The project lints itself: `install.sh`-installed or built binary:

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
  application/                use cases: init, install, run, test, sanitizer, deps, install-toolchain, generate-workflow
  application/checks/         concrete checks: shell, clang-format, clang-tidy, compiler, build, git-diff, cppcheck, gcc-analyzer, clang-static-analyzer, iwyu
  domain/                     config, check, workflow, dependency models + defaults, exit codes
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

Config (`[[checks]]`) selects the runtime behavior by **command basename**
through the `make_check` factory (`src/application/run_checks_use_case.cpp`).
Known commands map to a subclass of `domain::check::Check`; everything else
runs as `ShellCheck`.

Current check types: `clang-format`, `clang-tidy`, compilers (`gcc`, `g++`,
`clang`, `clang++`, `cc`, `c++`), `cmake`, `git`, `cppcheck`, `gcc-analyzer`,
`clang-static-analyzer`, `include-what-you-use`/`iwyu`.

To add a new specialized check type:

1. Subclass `Check` in `application/checks/` (e.g. `my_tool_check.hpp/cpp`).
2. Implement `execute(files, shell, verbose, dry_run)` → `CheckResult`.
3. Override `validate(repo_root)` if the tool needs a config file or other
   precondition (see `ClangTidyCheck` for the pattern).
4. Register the command basename in `make_check`.
5. Add the new files to the `sniffercommit_lib` sources in `CMakeLists.txt`.

```cpp
class MyToolCheck final : public domain::check::Check {
 public:
  explicit MyToolCheck(const domain::config::Check& cfg) : Check(cfg) {}

  [[nodiscard]] domain::check::CheckResult execute(
      const std::vector<std::string>& files, domain::ports::IShellExecutor* shell,
      bool verbose, bool dry_run) const override {
    // use command_line(files) + shell->exec_captured(), return { exit_code, output }
  }
};
```
