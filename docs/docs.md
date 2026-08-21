# metis

**Fast, C++20-powered pre-commit hook and CI generator.**

metis is a static binary that replaces Python/Node-based pre-commit frameworks. It reads a TOML configuration file, generates a `.git/hooks/pre-commit` Bash script and/or a GitHub Actions / GitLab CI workflow, and can also execute checks directly via the `run` subcommand. It is designed for C/C++ projects that need clang-format, trailing-whitespace detection, or any arbitrary command-based linter, with zero runtime dependencies beyond the operating system.

---

## Table of Contents

- [Project Structure](#project-structure)
- [Architecture Overview](#architecture-overview)
- [Build System](#build-system)
- [Configuration Format](#configuration-format)
- [CLI Interface](#cli-interface)
- [Component Breakdown](#component-breakdown)
  - [argparse.hpp: CLI Argument Parser](#argparsehpp-cli-argument-parser)
  - [Domain Layer](#domain-layer)
  - [Application Layer](#application-layer)
  - [Infrastructure Layer](#infrastructure-layer)
  - [Generators](#generators)
  - [Presentation Layer](#presentation-layer)
  - [main.cpp: Entry Point & Command Dispatch](#maincpp-entry-point--command-dispatch)
- [Data Flow](#data-flow)
- [Design Decisions & Trade-offs](#design-decisions--trade-offs)
- [Build & Debug](#build--debug)

---

## Project Structure

```
metis/
├── .metis.toml           # Example/default configuration
├── CMakeLists.txt                # CMake build system definition
├── LICENSE                       # MIT License
├── README.md
├── cmake/
│   └── config.hpp.in             # CMake configure_file template for build metadata
├── include/
│   └── metis/
│       ├── argparse.hpp          # Interface-only CLI argument parser
│       ├── glob_match.hpp        # Glob pattern matching
│       ├── spinner.hpp           # Terminal spinner animation
│       ├── util.hpp              # Shared utilities (shell_escape, etc.)
│       ├── domain/
│       │   ├── check.hpp         # Check base class, CheckResult
│       │   ├── config.hpp        # ProjectConfig, Check structs
│       │   ├── error_codes.hpp   # Typed exit codes
│       │   ├── workflow.hpp      # Platform enum, WorkflowConfig, workflow generation
│       │   └── ports/
│       │       ├── config_repository.hpp   # IConfigRepository interface
│       │       ├── file_system.hpp         # IFileSystem interface
│       │       ├── git_repository.hpp      # IGitRepository interface
│       │       ├── shell_executor.hpp      # IShellExecutor interface
│       │       ├── toolchain_provider.hpp  # IToolchainProvider interface
│       │       ├── http_client.hpp         # IHttpClient interface
│       │       └── archive_extractor.hpp   # IArchiveExtractor interface
│       ├── application/
│       │   ├── init_use_case.hpp            # InitUseCase (init subcommand)
│       │   ├── install_use_case.hpp         # InstallUseCase (install subcommand)
│       │   ├── install_toolchain_use_case.hpp # InstallToolchainUseCase (install-compiler)
│       │   ├── run_checks_use_case.hpp      # RunChecksUseCase (run subcommand)
│       │   ├── test_checks_use_case.hpp     # TestChecksUseCase (test subcommand)
│       │   ├── sanitizer_checks_use_case.hpp # SanitizerChecksUseCase (sanitizer subcommand)
│       │   ├── generate_workflow_use_case.hpp # GenerateWorkflowUseCase
│       │   ├── dependency_check_use_case.hpp  # DependencyCheckUseCase
│       │   ├── dependency.hpp                 # Dependency, DependencyCheckResult
│       │   └── checks/                      # concrete Check implementations
│       │       ├── shell_check.hpp          # ShellCheck (custom commands)
│       │       ├── clang_format_check.hpp   # ClangFormatCheck
│       │       ├── clang_tidy_check.hpp     # ClangTidyCheck
│       │       ├── compiler_check.hpp       # CompilerCheck
│       │       ├── build_check.hpp          # BuildCheck
│       │       ├── git_diff_check.hpp       # GitDiffCheck
│       │       ├── cppcheck_check.hpp       # CppcheckCheck
│       │       ├── gcc_analyzer_check.hpp   # GccAnalyzerCheck
│       │       ├── clang_static_analyzer_check.hpp # ClangStaticAnalyzerCheck
│       │       └── iwyu_check.hpp           # IwyuCheck
│       ├── generators/
│       │   ├── clang_format_generator.hpp   # .clang-format content generation
│       │   ├── clang_tidy_generator.hpp     # .clang-tidy content generation
│       │   ├── cmake_generator.hpp          # CMakeLists.txt content generation
│       │   └── conan_generator.hpp          # conanfile.py content generation
│       ├── infrastructure/
│       │   ├── toml_config_repository.hpp   # TOML config read/write
│       │   ├── os_file_system.hpp           # OS filesystem operations
│       │   ├── cli_git_repository.hpp       # Git CLI wrapper
│       │   ├── process_shell_executor.hpp   # popen/fork wrapper
│       │   ├── curl_http_client.hpp         # curl/wget HTTP download adapter
│       │   ├── tar_archive_extractor.hpp    # tar archive extraction adapter
│       │   ├── zip_archive_extractor.hpp    # unzip/powershell extraction adapter
│       │   ├── posix_toolchain_provider.hpp  # GCC install via package manager
│       │   ├── windows_gcc_provider.hpp     # MinGW-w64 download + install
│       │   ├── windows_clang_provider.hpp   # LLVM/Clang download + install
│       │   └── toolchain_factory.hpp        # Platform-conditional provider factory
│       └── presentation/
│           └── interactive_init.hpp         # TUI prompts for init wizard
├── src/
│   ├── main.cpp                  # Entry point, CLI dispatch
│   ├── glob_match.cpp
│   ├── spinner.cpp
│   ├── util.cpp
│   ├── argparse.cpp               # ArgParser implementation
│   ├── domain/
│   │   ├── check.cpp
│   │   ├── config.cpp
│   │   └── workflow.cpp
│   ├── application/
│   │   ├── init_use_case.cpp
│   │   ├── install_use_case.cpp
│   │   ├── install_toolchain_use_case.cpp
│   │   ├── run_checks_use_case.cpp
│   │   ├── test_checks_use_case.cpp
│   │   ├── sanitizer_checks_use_case.cpp
│   │   ├── dependency_check_use_case.cpp
│   │   ├── generate_workflow_use_case.cpp
│   │   └── checks/
│   │       ├── shell_check.cpp
│   │       ├── clang_format_check.cpp
│   │       ├── clang_tidy_check.cpp
│   │       ├── compiler_check.cpp
│   │       ├── build_check.cpp
│   │       ├── git_diff_check.cpp
│   │       ├── cppcheck_check.cpp
│   │       ├── gcc_analyzer_check.cpp
│   │       ├── clang_static_analyzer_check.cpp
│   │       └── iwyu_check.cpp
│   ├── generators/
│   │   ├── clang_format_generator.cpp
│   │   ├── clang_tidy_generator.cpp
│   │   ├── cmake_generator.cpp
│   │   └── conan_generator.cpp
│   ├── infrastructure/
│   │   ├── toml_config_repository.cpp
│   │   ├── os_file_system.cpp
│   │   ├── cli_git_repository.cpp
│   │   ├── process_shell_executor.cpp
│   │   ├── curl_http_client.cpp
│   │   ├── tar_archive_extractor.cpp
│   │   ├── zip_archive_extractor.cpp
│   │   ├── posix_toolchain_provider.cpp
│   │   ├── windows_gcc_provider.cpp
│   │   ├── windows_clang_provider.cpp
│   │   └── toolchain_factory.cpp
│   └── presentation/
│       └── interactive_init.cpp
└── docs/
    └── docs.md                   # This file
```

---

## Architecture Overview

metis follows a **hexagonal architecture** (ports & adapters) with clear layer separation:

```
┌─────────────────────────────────────────────────────┐
│                  presentation/                       │
│         interactive_init (TUI prompts)              │
├─────────────────────────────────────────────────────┤
│                  application/                        │
│   InitUseCase · InstallUseCase · RunChecksUseCase   │
│   TestChecksUseCase · SanitizerChecksUseCase        │
│   DependencyCheckUseCase                            │
│    GenerateWorkflowUseCase · InstallToolchainUseCase │
├─────────────────────────────────────────────────────┤
│                   domain/                            │
│   ProjectConfig · Check · Workflow · ExitCode        │
│                    ports/                            │
│   IConfigRepository · IFileSystem · IGitRepository   │
│   IShellExecutor · IToolchainProvider               │
│   IHttpClient · IArchiveExtractor                   │
├─────────────────────────────────────────────────────┤
│               infrastructure/                        │
│  TomlConfigRepository · OsFileSystem                │
│  CliGitRepository · ProcessShellExecutor            │
│  CurlHttpClient · TarArchiveExtractor               │
│  ZipArchiveExtractor · PosixToolchainProvider      │
│  WindowsGccProvider · WindowsClangProvider         │
│  ToolchainFactory                                  │
├─────────────────────────────────────────────────────┤
│                 generators/                          │
│  clang_format · clang_tidy · cmake · conan          │
│            (pure string generation)                  │
├─────────────────────────────────────────────────────┤
│              src/main.cpp (CLI entry)                │
│         ArgParser · subcommand dispatch              │
└─────────────────────────────────────────────────────┘
```

1. **`main.cpp`** parses CLI arguments via `argparse.hpp`, determines the active subcommand, wires infrastructure adapters, and delegates to use cases.
2. **Domain layer** (`domain/`) defines core data structures (`ProjectConfig`, `Check`, `WorkflowConfig`) and port interfaces (`IConfigRepository`, `IFileSystem`, `IGitRepository`, `IShellExecutor`, `IToolchainProvider`, `IHttpClient`, `IArchiveExtractor`).
3. **Application layer** (`application/`) contains use cases that orchestrate domain logic. Each use case receives port interfaces via constructor injection.
4. **Infrastructure layer** (`infrastructure/`) implements the port interfaces with real OS/git/TOML/curl operations.
5. **Generators** (`generators/`) are stateless free functions that produce string content for `.clang-format`, `.clang-tidy`, `CMakeLists.txt`, and `conanfile.py`.
6. **Presentation layer** (`presentation/`) handles interactive TUI prompts for the `init` wizard.

---

## Namespace Structure

```
metis
├── domain::config     : ProjectConfig, Check, config string generation
├── domain::check      : Check base class, CheckResult
├── domain::workflow   : Platform, WorkflowConfig, workflow generation
├── domain::ports      : IConfigRepository, IFileSystem, IGitRepository, IShellExecutor,
│                         IToolchainProvider, IHttpClient, IArchiveExtractor
├── application        : InitUseCase, InstallUseCase, RunChecksUseCase,
│                         TestChecksUseCase, SanitizerChecksUseCase,
│                         DependencyCheckUseCase,
│                         GenerateWorkflowUseCase, InstallToolchainUseCase
├── application::checks: ShellCheck, ClangFormatCheck, ClangTidyCheck, CompilerCheck,
│                         BuildCheck, GitDiffCheck
├── generators         : clang_format, clang_tidy, cmake, conan generators (free functions)
├── infrastructure     : TomlConfigRepository, OsFileSystem, CliGitRepository,
│                         ProcessShellExecutor, CurlHttpClient, TarArchiveExtractor,
│                         ZipArchiveExtractor, PosixToolchainProvider, WindowsGccProvider,
│                         WindowsClangProvider, ToolchainFactory
├── presentation       : interactive_init
└── (global)           : ArgParser, glob_match, spinner, util
```

## Exit Codes

| Code | Enum | Meaning |
|------|------|---------|
| `0` | `SUCCESS` | All checks passed, or command completed normally |
| `1` | `GENERAL_ERROR` | One or more checks failed, or a CLI/config error occurred |
| `2` | `INVALID_ARGUMENTS` | Invalid CLI arguments |
| `3` | `CONFIG_ERROR` | Configuration validation failed |
| `4` | `CHECK_FAILURE` | One or more checks failed |
| `5` | `FORMAT_FAILURE` | Formatter check failed |
| `6` | `MISSING_DEPENDENCY` | Required tool not found |
| `7` | `NOT_A_GIT_REPO` | Not inside a git repository |
| `8` | `FILESYSTEM_ERROR` | Filesystem operation failed |
| `9` | `HOOK_INSTALL_ERROR` | Failed to install pre-commit hook |
| `10` | `WORKFLOW_GENERATION_ERROR` | Failed to generate CI workflow |
| `11` | `TOOLCHAIN_INSTALL_ERROR` | Failed to install compiler toolchain |
| `12` | `UNSUPPORTED_PLATFORM` | Compiler not available for this platform |
| `13` | `UNSUPPORTED_CPP_STANDARD` | Compiler doesn't support requested C++ standard |
| `14` | `TEST_FAILURE` | Test execution failed |
| `15` | `TEST_BUILD_FAILURE` | Build before tests failed |
| `16` | `TEST_TIMEOUT` | Tests timed out |
| `17` | `COVERAGE_THRESHOLD_NOT_MET` | Coverage below configured threshold |
| `18` | `SANITIZER_BUILD_FAILURE` | Build with sanitizer failed |
| `19` | `SANITIZER_TEST_FAILURE` | Tests with sanitizer failed |

---

## Build System

### Requirements

- **CMake** ≥ 3.20
- **C++20** compiler (GCC ≥ 11, Clang ≥ 14, MSVC ≥ 2022 17.0)
- **Git** (for dependency fetching and commit embedding)

### Dependencies (fetched automatically via FetchContent)

| Dependency | Purpose | Version |
|------------|---------|---------|
| [tomlplusplus](https://github.com/marzer/tomlplusplus) | TOML config file parsing | v3.4.0 |
| [fmt](https://github.com/fmtlib/fmt) | Type-safe string formatting | 11.0.2 |

Both are fetched at configure time via `FetchContent_Declare` with `GIT_SHALLOW ON`. No system installation is required.

### Build Commands

```bash
# Release build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel

# Debug build with sanitizers
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DMETIS_ENABLE_SANITIZERS=ON
cmake --build build --parallel
```

### CMake Options

| Option | Default | Description |
|--------|---------|-------------|
| `METIS_ENABLE_SANITIZERS` | `OFF` | Enable AddressSanitizer + UndefinedBehaviorSanitizer (Debug only) |
| `METIS_BUILD_TESTS` | `ON` | Build unit test suite |
| `METIS_USE_SYSTEM_FMT` | `OFF` | Use system-installed fmt instead of FetchContent |
| `METIS_USE_SYSTEM_TOMLPLUSPLUS` | `OFF` | Use system-installed tomlplusplus instead of FetchContent |

### Compiler Warnings & Hardening

In GCC/Clang builds, the following warnings are enabled:
`-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Wnon-virtual-dtor -Wold-style-cast -Wcast-align -Wunused -Woverloaded-virtual -Wnull-dereference`

Plus hardening flags in non-Windows builds:
- `-Wl,-z,relro,-z,now` (RELRO + BIND_NOW)
- `-Wl,-z,noexecstack` (non-executable stack, Darwin excluded)
- `-fstack-protector-strong` (GCC only)

In MSVC builds:
`/W4 /permissive- /sdl /DYNAMICBASE /NXCOMPAT`

### Platform Detection

| `CMAKE_SYSTEM_NAME` | `METIS_PLATFORM` | Define |
|---------------------|--------------------------|--------|
| `Linux` (no WSL) | `linux` | - |
| `Linux` (WSL) | `wsl` | `METIS_PLATFORM_WSL` |
| `Darwin` | `macos` | `METIS_PLATFORM_MACOS` |
| `Windows` | `windows` | `METIS_PLATFORM_WINDOWS` |

### Build Metadata

During configuration, `cmake/config.hpp.in` is processed into `build/generated/metis/config_build.hpp`, embedding:
- Version string (from `CMakeLists.txt`)
- Platform (`linux`, `macos`, `windows`, `wsl`)
- Compiler ID and version
- Git commit hash (short)
- Build timestamp

These are accessible via `metis::BuildInfo`:

```cpp
struct BuildInfo {
  static constexpr const char *version = METIS_VERSION;
  static constexpr const char *platform = METIS_PLATFORM;
  static constexpr const char *compiler = METIS_COMPILER;
  static constexpr const char *git_commit = METIS_GIT_COMMIT;
  static constexpr const char *build_timestamp = METIS_BUILD_TIMESTAMP;
};
```

### Out-of-Source Build Enforcement

The CMakeLists.txt explicitly blocks in-source builds with a fatal error message if `CMAKE_SOURCE_DIR == CMAKE_BINARY_DIR`.

---

## Configuration Format

metis uses a TOML configuration file (default: `.metis.toml` in the current working directory).

### Schema

```toml
[project]
name = "my-project"

[[checks]]
name = "clang-format"
description = "Format C++ files"
enabled = true
command = "clang-format"
args = ["-i", "--fallback-style=Google", "-style=file"]
patterns = ["*.cpp", "*.hpp", "*.h", "*.cc"]
timeout = 30
severity = "warning"

[[checks]]
name = "trailing-whitespace"
command = "grep"
args = ["-E", "--text", "[[:space:]]+$"]
patterns = ["*"]

[exclude]
paths = ["build/", "third_party/", ".git/"]

[output]
local_hook = true
github_actions = false
gitlab_ci = false

[execution]
parallel = true

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

### Sections

| Section | Key | Type | Default | Description |
|---------|-----|------|---------|-------------|
| `[project]` | `name` | string | `"unnamed"` | Project name, embedded in generated scripts |
| `[[checks]]` | `name` | string | _required_ | Human-readable check name |
| `[[checks]]` | `description` | string | `""` | Free-text description of the check |
| `[[checks]]` | `enabled` | bool | `true` | Set `false` to skip the check entirely |
| `[[checks]]` | `command` | string | _required_ | Executable to run |
| `[[checks]]` | `args` | string[] | `[]` | Static arguments passed before the file path |
| `[[checks]]` | `patterns` | string[] | `[]` | Glob patterns to match files; `"*"` matches everything |
| `[[checks]]` | `timeout` | int | `0` | Max seconds for the check (`0` = no limit) |
| `[[checks]]` | `severity` | string | `"error"` | `error`, `warning`, or `info` |
| `[exclude]` | `paths` | string[] | `[]` | Path prefixes to exclude from checks |
| `[output]` | `local_hook` | bool | `true` | Generate `.git/hooks/pre-commit` during `install` |
| `[output]` | `github_actions` | bool | `false` | Generate `.github/workflows/metis.yml` during `install` |
| `[output]` | `gitlab_ci` | bool | `false` | Generate `.gitlab-ci.yml` during `install` |
| `[execution]` | `parallel` | bool | `true` | Run checks concurrently (Bash background jobs) |
| `[test]` | `build_dir` | string | `"build"` | CMake build directory for ctest |
| `[test]` | `coverage` | bool | `false` | Run coverage reporting |
| `[test]` | `line_threshold` | float | `80.0` | Minimum line coverage % |
| `[test]` | `branch_threshold` | float | `70.0` | Minimum branch coverage % |
| `[test]` | `function_threshold` | float | `90.0` | Minimum function coverage % |
| `[sanitizers]` | `enabled` | bool | `false` | Enable sanitizer checks |
| `[sanitizers]` | `types` | string[] | `["address", "undefined"]` | Sanitizer types to enable |
| `[sanitizers]` | `build_dir` | string | `"build"` | Build directory for sanitizer builds |
| `[sanitizers]` | `timeout` | int | `0` | Max seconds per sanitizer test (`0` = no limit) |

### Pattern Matching

metis supports a limited set of glob patterns in `glob_match.cpp`:

| Pattern | Semantics |
|---------|-----------|
| `*` | Matches every file |
| `*.cpp` | Matches any file ending with `.cpp` |
| `src/` | Prefix match: matches `src/foo.cpp`, `src/bar/baz.h` |
| `include/**` | Directory prefix: matches any path under `include/` |
| `**/suffix` | Suffix match: matches any path ending with `/suffix` |

Exclusion patterns in `is_excluded()` follow similar rules:
- `"build/"`: excludes any path starting with `build/` (trailing slash normalized)
- `"*.log"`: excludes files ending with `.log`
- `"third_party/"`: excludes the entire `third_party/` directory tree

### Validation Rules

When loading a configuration file, `ProjectConfig::validate()` enforces these rules:

1. **Project name** cannot be empty.
2. At least one `[[checks]]` entry is required.
3. Every check must have a non-empty **name**.
4. Every check must have a non-empty **command**.
5. Duplicate check names are rejected.
6. `timeout` cannot be negative.
7. `severity` must be `error`, `warning`, or `info`.

Validation errors return a descriptive error string. All validation is also available via `ProjectConfig::is_valid()`.

---

## CLI Interface

```
metis - Fast C++20-powered pre-commit & CI generator

Usage:
  metis [OPTIONS] <SUBCOMMAND> [ARGS]

Core Workflow:
  init            Create default .metis.toml
  install         Generate & install .git/hooks/pre-commit
  generate-gha    Output GitHub Actions workflow
  generate-gitlab Output GitLab CI workflow
  run             Execute checks on files
  install-compiler Download and install a C++ toolchain
  test            Run test and optional coverage checks
  sanitizer       Run sanitizer checks (ASan, UBSan, TSan, LSan)
  deps            Check project dependencies (Conan, vcpkg, CMake FetchContent)

Subcommands:
  init
      Create:
        - .metis.toml
        - .clang-format

      Options:
        --style <google|llvm|chromium|mozilla|webkit|microsoft|gnu>
        --name <project-name>
        --indent-width <n>
        --column-limit <n>
        --pointer-alignment <Left|Right|Middle>
        --brace-style <Attach|Allman|...
        --enable-clang-tidy, --tidy
        --tidy-preset <minimal|standard|strict|custom>
        --tidy-severity <note|warning|error>
        --tidy-header-filter <0|1|2>
        --enable-cmake, --cmake
        --cmake-cpp-standard <17|20|23>
        --cmake-target-type <executable|static|shared|header-only>
        --cmake-enable-testing
        --cmake-enable-sanitizers
        --generate-src
        --enable-compiler-checks, --compiler-checks
        --compiler <g++|clang++|gcc|clang>
        --compiler-cpp-standard <17|20|23|26>
        --compiler-werror / --compiler-no-werror
        --compiler-debug-and-release

  install
      Generate and install:
        .git/hooks/pre-commit
        .github/workflows/metis.yml  (if github_actions = true)
        .gitlab-ci.yml                        (if gitlab_ci = true)

      Usage:
        metis install

  run
      Execute configured checks.

      Modes:
        --all-files
        <explicit files>

      Usage:
        metis run --all-files
        metis run src/main.cpp
        metis run --format --all-files

  generate-gha
      Generate GitHub Actions workflow.

      Usage:
        metis generate-gha
        metis generate-gha > .github/workflows/metis.yml

  generate-gitlab
      Generate GitLab CI workflow.

      Usage:
        metis generate-gitlab
        metis generate-gitlab > .gitlab-ci.yml

  install-compiler
      Download and install a C++ toolchain.

      Options:
        --compiler <gcc|clang>         [default: gcc]
        --version <version>
        --cpp-standard <17|20|23|26>   [default: 20]
        --prefix <path>
        --force
        --dry-run, -n

      Usage:
        metis install-compiler
        metis install-compiler --compiler gcc --cpp-standard 20
        metis install-compiler --compiler clang --dry-run

  test
      Run ctest and optional coverage checks.

      Options:
        --coverage
        --verbose, -V
        <build-dir>                   [default: build]

      Usage:
        metis test
        metis test --coverage
        metis test --verbose build/

  sanitizer
      Run sanitizer checks (ASan, UBSan, TSan, LSan).

      Options:
        --verbose, -V

      Usage:
        metis sanitizer
        metis sanitizer --verbose

Global Options:
  -c, --config <value>  Config file path [default: .metis.toml]
  -v, --version         Show version
  -h, --help            Show help message
```

### Subcommand Details

#### `init`

Creates both `.metis.toml` and `.clang-format` with sensible defaults in the current directory. The `clang-format` check in the generated TOML includes `--fallback-style=<style>` matching the chosen formatter style.

**Options:**

| Flag | Description |
|------|-------------|
| `--interactive`, `-i` | Force interactive wizard (default when no flags) |
| `--style <name>` | Formatter style: `google`, `llvm`, `chromium`, `mozilla`, `webkit`, `microsoft`, `gnu` (default: `google`) |
| `--name <name>` | Project name (default: current directory name) |
| `--indent-width <n>` | Indentation width for `.clang-format` (default: `2`) |
| `--column-limit <n>` | Column limit for `.clang-format` (default: `100`) |
| `--pointer-alignment <s>` | Pointer alignment style (default: `Left`) |
| `--brace-style <s>` | Brace placement style (default: `Attach`) |
| `--enable-clang-tidy`, `--tidy` | Enable clang-tidy static analysis check (generates `.clang-tidy`) |
| `--tidy-preset <name>` | Tidy check preset: `minimal`, `standard`, `strict`, `custom` (default: `standard`) |
| `--tidy-severity <s>` | Warnings-as-errors level: `note` (off), `warning` (compiler only), `error` (all) (default: `error`) |
| `--tidy-header-filter <0\|1\|2>` | Header filtering: `0`=none, `1`=project, `2`=all (default: `1`) |
| `--enable-cmake`, `--cmake` | Enable CMake scaffolding (generates `CMakeLists.txt` + `src/main.cpp`) |
| `--enable-conan` | Generate `conanfile.py` alongside CMakeLists.txt (uses `find_package` instead of `FetchContent`) |
| `--cmake-cpp-standard <n>` | C++ standard: `17`, `20`, `23` (default: `20`) |
| `--cmake-target-type <t>` | Build target: `executable`, `static`, `shared`, `header-only` (default: `executable`) |
| `--cmake-enable-testing` | Add testing infrastructure to generated `CMakeLists.txt` |
| `--cmake-enable-sanitizers` | Enable AddressSanitizer + UBSan in Debug builds |
| `--generate-src` | Generate `src/main.cpp` without CMake (implied by `--enable-cmake`) |

When `--enable-clang-tidy` is used, three files are created: `.metis.toml`, `.clang-format`, and `.clang-tidy` with the chosen preset.

Project name is auto-detected from the current working directory name if `--name` is not provided. Existing files are overwritten.

#### `install`

1. Loads config via `TomlConfigRepository::load()`.
2. Finds git root via `CliGitRepository::find_repo_root()`.
3. Generates a Bash pre-commit hook (via `InstallUseCase`).
4. Writes it to `.git/hooks/pre-commit` with executable permissions.
5. Optionally generates a GitHub Actions workflow and/or GitLab CI workflow.

Requires a valid `.metis.toml` in the current directory.

```bash
metis install
```

#### `generate-gha`

Generates only the GitHub Actions workflow file (`.github/workflows/metis.yml`). Always writes regardless of the config's `github_actions` setting.

The generated workflow includes:
- Triggers on push and pull_request to all branches
- Concurrency group with `cancel-in-progress: true`
- `contents: read` permission
- 10-minute timeout
- `actions/checkout@v4` with `fetch-depth: 0`
- Conditional `clang-format` installation step (only if any check uses clang-format)
- Runs `./metis run --all-files --verbose` with `set -euo pipefail`

```bash
metis generate-gha
metis generate-gha > .github/workflows/metis.yml
```

#### `generate-gitlab`

Generates only the GitLab CI workflow file (`.gitlab-ci.yml`). Always writes regardless of the config's `gitlab_ci` setting.

```bash
metis generate-gitlab
metis generate-gitlab > .gitlab-ci.yml
```

#### `run`

Executes checks directly from the binary (no generated script needed). Accepts the following flags:

| Flag | Description |
|------|-------------|
| `--all-files` | Run checks on all files tracked by git (via `git ls-files`) |
| `--verbose`, `-V` | Print detailed per-file execution output |
| `--detail` | Alias for `--verbose` |
| `--dry-run`, `-n` | List files that would be checked without running checks |
| `--format`, `-f` | Run clang-format in-place instead of checking |

**File source selection (mutually exclusive, listed in priority order):**

1. **Explicit files**: Positional arguments: `metis run src/main.cpp src/foo.cpp`
2. **`--all-files`**: All git-tracked files
3. **Default (no args)**: Staged files via `git diff --cached --name-only --diff-filter=ACM` (this is the implicit `--staged` mode)

---

## Component Breakdown

### argparse.hpp: CLI Argument Parser

**File:** `include/metis/argparse.hpp`

A **header-only, template-based** argument parser. No external dependency.

#### Key Design

- **`ArgParser` class** holds a list of `Option` structs, `Subcommand` structs, boolean flag stores, and an active subcommand string.
- **`add_option<T>()`** is a template method accepting a reference to a variable of type `T`. It stores a lambda that converts the CLI string value into `T` at parse time.
- **`add_flag()`** registers a boolean flag (no value argument).
- **`add_subcommand()`** registers a named subcommand with a description.
- **`parse()`** iterates `argv` once, checking for `--help`, `--version`, and subcommand matching.

#### Type Constraints

The `Parsable` concept requires `std::assignable_from<T&, T>` and `std::default_initializable<T>`. Supported types: `std::string`, `int`, `bool`.

### Domain Layer

#### config.hpp: Project Configuration

**File:** `include/metis/domain/config.hpp`

Namespace: `metis::domain::config`

```cpp
struct Check {
  std::string name;
  std::string description;
  std::string command;
  std::vector<std::string> args;
  std::vector<std::string> patterns;
  bool enabled = true;
  int timeout = 0;                     // seconds; 0 = no limit
  std::string severity = "error";      // error | warning | info
  [[nodiscard]] std::string validate() const noexcept;
};

struct ProjectConfig {
  std::string project_name = "unnamed";
  std::vector<Check> checks;
  std::vector<std::string> exclude_paths;
  bool generate_local_hook = true;
  bool generate_gha = false;
  bool generate_gitlab_ci = false;
  bool parallel = true;

  struct TestConfig {
    std::string build_dir = "build";
    bool coverage = false;
    double line_threshold = 80.0;
    double branch_threshold = 70.0;
    double function_threshold = 90.0;
    int timeout = 0;
  };
  TestConfig test;

  struct SanitizerConfig {
    bool enabled = false;
    std::vector<std::string> types;
    std::string build_dir = "build";
    int timeout = 0;
  };
  SanitizerConfig sanitizer;

  [[nodiscard]] std::string validate() const noexcept;
  [[nodiscard]] bool is_valid() const noexcept;
  [[nodiscard]] bool has_command(std::string_view cmd) const noexcept;
};
```

Free functions for config string generation (no I/O):
- `generate_default_config()`: basic TOML template
- `generate_default_config_with_tidy()`: TOML with clang-tidy check
- `generate_default_config_with_cmake()`: TOML with cmake check
- `generate_compiler_checks()`: `[[checks]]` blocks that syntax-check files with a compiler
- `generate_sanitizer_config()`: `[sanitizers]` section with enabled flag and types

#### check.hpp: Generic Check Abstraction

**File:** `include/metis/domain/check.hpp`

Namespace: `metis::domain::check`

All runnable checks derive from `Check`. The base holds the config-derived
fields, provides getters, and defines the execution contract:

- Fields: `name`, `description`, `enabled`, `file_patterns`, `command`,
  `arguments`, `timeout`, `severity`.
- `validate(repo_root)`: virtual; checks preconditions (config files,
  toolchain) at build time. Default returns OK. A failure aborts the run with
  `CONFIG_ERROR` before any file is processed.
- `execute(files, shell, verbose, dry_run)`: pure virtual; performs the check
  and returns a `CheckResult { exit_code, output }`.
- `command_line(files)`: shared helper that assembles the shell command from
  `command`, `arguments`, and the file list.

#### workflow.hpp: CI/CD Workflow

**File:** `include/metis/domain/workflow.hpp`

Namespace: `metis::domain::workflow`

```cpp
enum class Platform : std::uint8_t { GithubAction, GitLabCI };

struct WorkflowConfig {
  Platform platform = Platform::GithubAction;
  std::string job_name = "Run metis checks";
  int timeout_minutes = 10;
  bool install_clang_format = false;
  bool install_clang_tidy = false;
  std::string binary_path = "./metis";
};
```

Key functions:
- `generate_workflow()`: dispatches to platform-specific generator
- `generate_github_actions()`: produces GitHub Actions YAML
- `generate_gitlab_ci()`: produces GitLab CI YAML
- `requires_clang_format()` / `requires_clang_tidy()`: check detection

#### ports/: Port Interfaces

Seven port interfaces define the contracts between layers:

| Port | File | Methods |
|------|------|---------|
| `IConfigRepository` | `config_repository.hpp` | `load()`, `find_git_root()` |
| `IFileSystem` | `file_system.hpp` | `exists()`, `create_directories()`, `write_file()`, `read_file()`, `remove()`, `set_permissions()`, `current_path()`, `absolute()` |
| `IGitRepository` | `git_repository.hpp` | `list_staged_files()`, `list_all_files()`, `find_repo_root()` |
| `IShellExecutor` | `shell_executor.hpp` | `exec()`, `exec_captured()`, `command_exists()` |
| `IToolchainProvider` | `toolchain_provider.hpp` | `is_installed()`, `get_version()`, `supports_cpp_standard()`, `max_supported_standard()`, `resolve_package()`, `install()`, `description()` |
| `IHttpClient` | `http_client.hpp` | `download()` |
| `IArchiveExtractor` | `archive_extractor.hpp` | `extract()` |

#### error_codes.hpp: Typed Exit Codes

**File:** `include/metis/domain/error_codes.hpp`

Provides a typed enum with 20 exit code values for consistent error reporting.

### Application Layer

#### init_use_case.hpp

**File:** `include/metis/application/init_use_case.hpp`

```cpp
struct InitOptions {
  std::string project_name;
  std::string style = "Google";
  int indent_width = 2;
  int column_limit = 100;
  std::string pointer_alignment = "Left";
  std::string brace_style = "Attach";
  bool enable_clang_tidy = false;
  std::string tidy_preset = "standard";
  std::string tidy_severity = "error";
  int tidy_header_filter = 1;
  bool enable_cmake = false;
  std::string cmake_cpp_standard = "20";
  std::string cmake_target_type = "executable";
  bool cmake_enable_testing = false;
  bool cmake_enable_sanitizers = false;
  bool cmake_enable_warnings = true;
  bool generate_source = true;
  std::vector<std::string> dependencies;
  bool enable_compiler_checks = false;
  std::string compiler = "g++";
  std::string compiler_cpp_standard = "20";
  std::vector<std::string> compiler_warnings = {"Wall", "Wextra", "Wpedantic"};
  bool compiler_werror = true;
  bool compiler_debug_and_release = false;
};

class InitUseCase {
  InitUseCase(std::unique_ptr<IConfigRepository>, std::unique_ptr<IFileSystem>);
  [[nodiscard]] InitResult execute(const std::filesystem::path& cwd, const InitOptions& opts);
};
```

Creates `.metis.toml`, `.clang-format`, optionally `.clang-tidy`, `CMakeLists.txt`, and `src/main.cpp`.

#### install_use_case.hpp

**File:** `include/metis/application/install_use_case.hpp`

```cpp
struct InstallResult {
  bool hook_installed = false;
  bool workflow_installed = false;
  std::string hook_path;
  std::string workflow_path;
  std::string error_message;
};

class InstallUseCase {
  InstallUseCase(std::unique_ptr<IFileSystem>, std::unique_ptr<IGitRepository>);
  [[nodiscard]] InstallResult execute(const std::filesystem::path& repo_root,
                                      const ProjectConfig& cfg);
};
```

Generates and installs the pre-commit hook and optionally CI workflows.

#### run_checks_use_case.hpp

**File:** `include/metis/application/run_checks_use_case.hpp`

```cpp
enum class FileSource : std::uint8_t { STAGED, ALL_REPO, EXPLICIT };
enum class RunMode : std::uint8_t { CHECK, FORMAT };

struct RunOptions {
  FileSource source = FileSource::STAGED;
  std::vector<std::string> explicit_files;
  bool verbose = false;
  bool dry_run = false;
  RunMode mode = RunMode::CHECK;
};

class RunChecksUseCase {
  RunChecksUseCase(std::unique_ptr<IShellExecutor>,
                   std::unique_ptr<IGitRepository>,
                   std::unique_ptr<IFileSystem>);
  [[nodiscard]] int execute(const ProjectConfig& cfg, const RunOptions& opts);
};
```

Collects files from git, filters by patterns, and runs checks. Returns `0` if all pass, `1` otherwise.

Each config `Check` is dispatched through the `make_check` factory to the
matching concrete check (see below); checks run sequentially or in parallel
via `std::async` (`[execution] parallel`).

#### checks/: Check Implementations

**Files:** `include/metis/application/checks/` + `src/application/checks/`

Namespace: `metis::application::checks`

Each concrete check subclasses `domain::Check`. The right class is selected by
**command basename** in the `make_check` factory (`run_checks_use_case.cpp`);
unknown commands fall back to `ShellCheck`.

| Command basename | Class | Behavior |
|---|---|---|
| `clang-format` | `ClangFormatCheck` | In-place formatting; `Formatted`/`Clean` per file; exit `0` after fixing |
| `clang-tidy` | `ClangTidyCheck` | Static analysis; requires `.clang-tidy` |
| `gcc`, `g++`, `clang`, `clang++`, `cc`, `c++` (incl. versioned) | `CompilerCheck` | Per-file syntax compile; forces `-fsyntax-only` unless a mode flag is set |
| `cmake` | `BuildCheck` | Runs once, no file args (e.g. `cmake --build`) |
| `git` | `GitDiffCheck` | Runs once against the working tree (e.g. `git diff --check`) |
| `cppcheck` | `CppcheckCheck` | Static analysis via cppcheck; batches all files |
| `gcc-analyzer` | `GccAnalyzerCheck` | GCC static analyzer (`-fanalyzer`); per-file |
| `clang-static-analyzer` | `ClangStaticAnalyzerCheck` | Clang static analyzer; per-file |
| `include-what-you-use`, `iwyu` | `IwyuCheck` | Include-what-you-use analysis; per-file |
| anything else | `ShellCheck` | Custom shell command; inverts `grep`/`rg` exit codes; batches multi-file tools |

Runner flow: `make_check(config)` → `validate()` (build time) → `execute()`
(run time).

#### generate_workflow_use_case.hpp

**File:** `include/metis/application/generate_workflow_use_case.hpp`

```cpp
class GenerateWorkflowUseCase {
  explicit GenerateWorkflowUseCase(std::unique_ptr<IFileSystem> file_system);
  [[nodiscard]] bool execute(const ProjectConfig& cfg,
                             const std::filesystem::path& repo_root,
                             Platform platform = Platform::GithubAction);
};
```

Single dependency: `IFileSystem`. Dispatches to platform-specific workflow generators.

#### install_toolchain_use_case.hpp

**File:** `include/metis/application/install_toolchain_use_case.hpp`

```cpp
struct InstallToolchainOptions {
  std::string compiler_ = "gcc";
  std::string version_;
  domain::ports::CppStandard cpp_standard_ = domain::ports::CppStandard::CPP_20;
  std::filesystem::path install_prefix_;
  bool force_ = false;
  bool dry_run_ = false;
};

struct InstallToolchainResult {
  bool success_ = false;
  bool was_already_installed_ = false;
  std::string installed_path_;
  std::string version_;
  domain::ports::CppStandard installed_cpp_standard_ = domain::ports::CppStandard::CPP_20;
  std::string error_message_;
};

class InstallToolchainUseCase {
  InstallToolchainUseCase(std::unique_ptr<IToolchainProvider>,
                          std::unique_ptr<IHttpClient>,
                          std::unique_ptr<IArchiveExtractor>,
                          std::unique_ptr<IFileSystem>);
  [[nodiscard]] InstallToolchainResult execute(const InstallToolchainOptions& opts);
};
```

Downloads and installs a GCC toolchain. Flow:
1. If compiler is already installed and `--force` is not set → return early
2. Resolve the provider package (URL or package manager)
3. Download archive via `IHttpClient` (if URL is present)
4. Extract via `IArchiveExtractor` (if downloaded)
5. Install via `IToolchainProvider::install()`
6. Verify compiler is in PATH after installation

#### dependency_check_use_case.hpp

**File:** `include/metis/application/dependency_check_use_case.hpp`

```cpp
struct DependencyCheckOptions {
  bool verbose = false;
  bool generate_graph = false;
  std::string graph_output_path = "dependencies.dot";
};

class DependencyCheckUseCase {
  DependencyCheckUseCase(std::unique_ptr<IShellExecutor>,
                         std::unique_ptr<IFileSystem>);
  [[nodiscard]] DependencyCheckResult execute(
      const std::filesystem::path& repo_root,
      const DependencyCheckOptions& opts);
};
```

Parses Conan (`conanfile.py`), vcpkg (`vcpkg.json`), and CMake FetchContent
(`CMakeLists.txt`) dependencies. Validates semver, detects duplicates across
sources, and checks for missing lockfiles.

### Infrastructure Layer

Ten adapters implement the port interfaces:

| Adapter | Port | Implementation |
|---------|------|----------------|
| `TomlConfigRepository` | `IConfigRepository` | TOML parsing via `tomlplusplus`, config string generation |
| `OsFileSystem` | `IFileSystem` | `std::filesystem` operations |
| `CliGitRepository` | `IGitRepository` | `popen()` wrapping git CLI commands |
| `ProcessShellExecutor` | `IShellExecutor` | `popen()` / `fork()` / `std::system()` for command execution |
| `CurlHttpClient` | `IHttpClient` | Shells out to `curl` or `wget` for downloads |
| `TarArchiveExtractor` | `IArchiveExtractor` | `tar` command for `.tar.gz`/`.tar.xz`/`.tar.bz2` archives |
| `ZipArchiveExtractor` | `IArchiveExtractor` | `unzip` (Unix) or `powershell Expand-Archive` (Windows) |
| `PosixToolchainProvider` | `IToolchainProvider` | Detects package manager (apt/dnf/pacman/zypper) and installs GCC |
| `WindowsGccProvider` | `IToolchainProvider` | Downloads MinGW-w64 from WinLibs, extracts, and installs |
| `WindowsClangProvider` | `IToolchainProvider` | Downloads LLVM/Clang for Windows, extracts, and installs |
| `ToolchainFactory` | - | Static `create()` method returns the platform-appropriate provider |

### Generators

Stateless free functions in `metis::generators` namespace:

| Generator | Functions |
|-----------|-----------|
| `clang_format_generator` | `generate_clang_format(style, indent_width, column_limit, pointer_alignment, brace_style)`, `generate_clang_format_style(style)` |
| `clang_tidy_generator` | `generate_clang_tidy(preset, severity, header_filter_level)`, `get_preset_checks(preset)` |
| `cmake_generator` | `generate_cmake_lists(project_name, cpp_standard, target_type, enable_testing, enable_sanitizers, enable_warnings, enable_clang_tidy, enable_conan, dependencies)` |
| `conan_generator` | `generate_conanfile(project_name, cpp_standard, enable_testing, dependencies)` |

All return `std::string`: pure string generation, no I/O.

### main.cpp: Entry Point & Command Dispatch

**File:** `src/main.cpp`

#### Dispatch Flow

```
main()
├── preparse_config_path()  (extract --config before ArgParser)
├── Parse CLI args via ArgParser
├── subcommand == "init"
│   ├── Manual argv loop for init flags
│   ├── Wire TomlConfigRepository + OsFileSystem
│   ├── InitUseCase(config_repo, fs).execute(cwd, opts)
│   │   ├── Generates .metis.toml, .clang-format
│   │   ├── [if tidy] Generates .clang-tidy
│   │   ├── [if cmake] Generates CMakeLists.txt + src/main.cpp
│   │   └── Writes files via IConfigRepository/IFileSystem
├── subcommand == "install"
│   ├── Wire TomlConfigRepository + OsFileSystem + CliGitRepository
│   ├── TomlConfigRepository::load(config_path)
│   ├── InstallUseCase(fs, git).execute(repo_root, cfg)
│   │   ├── [if local_hook] Generate + validate + install hook
│   │   └── [if gha/gitlab] Generate + install CI workflow
├── subcommand == "generate-gha"
│   ├── Wire TomlConfigRepository + OsFileSystem + CliGitRepository
│   ├── Load config, find git root
│   └── GenerateWorkflowUseCase(fs).execute(cfg, root, Platform::GithubAction)
├── subcommand == "generate-gitlab"
│   ├── Wire TomlConfigRepository + OsFileSystem + CliGitRepository
│   ├── Load config, find git root
│   └── GenerateWorkflowUseCase(fs).execute(cfg, root, Platform::GitLabCI)
├── subcommand == "run"
│   ├── Manual argv re-parse for run flags
│   ├── Wire TomlConfigRepository + OsFileSystem + CliGitRepository + ProcessShellExecutor
│   ├── RunChecksUseCase(shell, git, fs).execute(cfg, opts)
├── subcommand == "install-compiler"
│   ├── Manual argv loop for install-compiler flags
│   ├── Wire ToolchainFactory → platform-appropriate provider
│   ├── Wire CurlHttpClient + TarArchiveExtractor/ZipArchiveExtractor + OsFileSystem
│   ├── InstallToolchainUseCase(provider, http, extractor, fs).execute(opts)
├── subcommand == "test"
│   ├── Manual argv loop for test flags
│   ├── Wire TomlConfigRepository + OsFileSystem
│   ├── Load config
│   └── TestChecksUseCase(shell, fs).execute(cfg, repo_root, coverage, verbose)
├── subcommand == "sanitizer"
│   ├── Wire TomlConfigRepository + OsFileSystem + ProcessShellExecutor
│   ├── Load config
│   └── SanitizerChecksUseCase(shell, fs).execute(cfg, repo_root, verbose)
├── subcommand == "deps"
│   ├── Wire TomlConfigRepository + OsFileSystem + ProcessShellExecutor
│   ├── Load config
│   └── DependencyCheckUseCase(shell, fs).execute(repo_root, opts)
└── fallback
    └── show_help()
```

#### `preparse_config_path()`: Early Config Resolution

Scans `argv` for `-c` / `--config` before `ArgParser` processes arguments, avoiding the limitation where global options are not parsed when a subcommand is present.

---

## Data Flow

### Run Flow

```
User runs: metis run --all-files

main.cpp
├── preparse_config_path() → ".metis.toml"
├── ArgParser::parse() → recognizes "run" subcommand
├── Manual argv loop → detects --all-files
├── Wire infrastructure adapters
├── TomlConfigRepository::load() → ProjectConfig
├── CliGitRepository::find_repo_root() → /path/to/repo
└── RunChecksUseCase::execute(cfg, opts)
    ├── collect_files()
    │   ├── git_repo_->list_all_files()
    │   └── Filter through glob_match + exclusions
    └── execute_checks()
        ├── For each check:
        │   ├── shell_->command_exists()
        │   ├── matches_pattern() per file
        │   ├── shell_escape() per argument
        │   └── shell_->exec_captured()
        └── Return 0 (all pass) or 1 (any fail)
```

### Install Flow

```
User runs: metis install

main.cpp
├── preparse_config_path() → ".metis.toml"
├── Wire TomlConfigRepository + OsFileSystem + CliGitRepository
├── TomlConfigRepository::load() → ProjectConfig
├── CliGitRepository::find_repo_root()
└── InstallUseCase::execute(repo_root, cfg)
    ├── [local_hook]
    │   ├── precommit::generate_hook(cfg)
    │   ├── precommit::validate_syntax(hook_content)
    │   └── precommit::install(repo_root, hook_content)
    └── [github_actions / gitlab_ci]
        ├── GenerateWorkflowUseCase::execute(cfg, root, platform)
        └── Write CI file via IFileSystem
```

---

## Design Decisions & Trade-offs

### Why a generated Bash hook instead of invoking the binary in the hook?

The generated Bash hook is self-contained: it does not require the metis binary to be installed on every developer machine or CI runner. A `git clone` + `metis install` is sufficient for setup. The trade-off is that the hook script contains generated logic that must be re-generated when the configuration changes.

### Why hexagonal architecture?

The ports & adapters pattern keeps the domain logic (config parsing, workflow generation) independent of OS-specific operations (filesystem, git CLI, TOML parsing). This makes the core logic testable with mock adapters and allows infrastructure implementations to be swapped without touching business logic.

### Why sequential execution in the binary but parallel in the hook?

The generated Bash hook uses `(...) &` background jobs for parallel execution, which is simple and reliable in a shell script. The C++ binary runs checks sequentially via `std::system()` because adding thread-based parallelism with proper output synchronization would increase complexity for limited benefit (the hook is the primary execution path; the `run` subcommand is for manual debugging).

### Why `popen()` for git commands instead of libgit2?

The project has zero runtime dependencies. Using `popen()` to shell out to `git` avoids linking against libgit2 or similar libraries, keeping the binary small and build times fast. The trade-off is reliance on `git` being in `PATH`.

### Why `std::system()` for check execution instead of `fork/exec`?

`std::system()` is portable and simple. For a developer tool where checks are linters that run in <1 second, the overhead of `fork/exec` vs. `std::system()` is negligible. The generated Bash hook uses direct command execution, not `std::system()`.

### Why does `init` generate a separate `.clang-format` alongside `.metis.toml`?

Some linters (notably `clang-format`) look for a `.clang-format` file in the project root to determine formatting rules. Generating both files from a single `init` command ensures that the `clang-format` check in the TOML (`--style=file` + `--fallback-style=<style>`) has a corresponding config file to read.

### Why no subcommand-specific `--config` support?

The `--config` option is registered globally on the `ArgParser`. Because `parse()` returns early when a subcommand is found, options are not parsed when a subcommand is present. The codebase mitigates this with `preparse_config_path()`, which scans `argv` for `--config` before `ArgParser` processes arguments.

---

## Error Handling Patterns

metis uses a consistent error handling strategy across all modules:

| Pattern | Where | Description |
|---------|-------|-------------|
| `std::runtime_error` throws | `TomlConfigRepository::load()`, `find_git_root()`, `generate_workflow()` | Fatal errors where the operation cannot continue. All caught by `main()`'s top-level try-catch. |
| Result structs with error strings | `InitUseCase::execute()` / `InstallUseCase::execute()` | Non-fatal or recoverable errors. Returns an `InitResult`/`InstallResult` with `success=false` and `error_message` populated. |
| `bool` return + early exit | `ArgParser::parse()`, `validate_syntax()` | Validation failures where the caller just needs pass/fail. |
| Stream-based warnings | `RunChecksUseCase` | Non-fatal per-check failures (e.g., command not found) that don't abort the full run. |

The top-level `main()` wraps all dispatch in a single try-catch:

```cpp
try {
  // subcommand dispatch
} catch (const std::exception& error) {
  std::cerr << "[ERROR] " << error.what() << "\n";
  return 1;
} catch (...) {
  std::cerr << "[ERROR] Unknown fatal error occurred\n";
  return 1;
}
```

---

## Testing & CI

### Testing Approach

metis uses **GoogleTest** for unit testing. Tests are located in the `tests/` directory and are executed via CTest:

```bash
# Run all tests
ctest --test-dir build --output-on-failure

# Run specific test suite
ctest --test-dir build --test-suite="SanitizerChecksTest"
```

### Test Coverage

| Test File | What it verifies |
|-----------|-----------------|
| `test_binary_runtime.cpp` | Binary version output and basic invocation |
| `test_config_generators.cpp` | Config generator functions and ProjectConfig validation |
| `test_config_manager.cpp` | TOML config loading and validation |
| `test_dependency_checks.cpp` | Dependency check use case (Conan, CMake parsing, duplicates, lockfiles) |
| `test_format_mode.cpp` | Format mode execution |
| `test_glob_match.cpp` | Glob pattern matching |
| `test_precommit_domain.cpp` | Pre-commit hook domain logic |
| `test_sanitizer_checks.cpp` | Sanitizer checks use case (empty types, unknown type, missing build dir) |
| `test_sha256_verification.cpp` | SHA-256 checksum verification for Windows installer |
| `test_test_checks_use_case.cpp` | Test checks use case (missing build dir) |
| `test_toolchain_install.cpp` | Toolchain installation flow |
| `test_tooling_config.cpp` | Tooling configuration generation |

### CI Pipeline

A GitHub Actions workflow (generated by `metis generate-gha`) runs on every push and pull request:

1. **Build**: cmake configure + build (Release and Debug)
2. **Lint**: `clang-tidy` with the project's own `.clang-tidy` config
3. **Test**: GoogleTest suite via CTest
4. **Package**: CPack DEB generation (Linux only)

---

## Build & Debug

### Debug Build with Sanitizers

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DMETIS_ENABLE_SANITIZERS=ON
cmake --build build --parallel
```

### Manual Hook Test

```bash
# Generate and install
./build/metis install

# Test the hook directly (outside git commit flow)
bash .git/hooks/pre-commit

# Inspect generated script
cat .git/hooks/pre-commit | less
```

### Direct Check Run (no hook needed)

```bash
# All tracked files
./build/metis run --all-files

# Specific files
./build/metis run src/main.cpp include/foo.hpp

# Dry-run (show what would be checked, no commands executed)
./build/metis run --dry-run --all-files

# Verbose output (print each shell command before execution)
./build/metis run --verbose src/main.cpp
```

### Typical Workflow

```bash
# 1. Initialize a project
cd my-project
metis init --style google

# 2. Run checks before committing
echo "int main(){}" > main.cpp
metis run main.cpp

# 3. Install the hook for automatic checks
metis install

# 4. Commit
git add main.cpp
git commit -m "feat: initial"

# 5. (CI) Generate workflows (enable in config, then re-install)
# Edit .metis.toml to set github_actions = true / gitlab_ci = true
metis install  # regenerates hook and workflows
```
