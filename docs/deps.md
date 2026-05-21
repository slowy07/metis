# Dependency Management

sniffercommit supports adding external C++ dependencies to your project via CMake [FetchContent](https://cmake.org/cmake/help/latest/module/FetchContent.html). Dependencies are declared during `sniffercommit init` and validated before any files are created.

---

## Overview

When you enable CMake scaffolding (`--enable-cmake` or interactive mode), sniffercommit can automatically generate `FetchContent_Declare` blocks in your `CMakeLists.txt`. Each dependency requires:

| Field | Description | Example |
|-------|-------------|---------|
| `name` | Package name (used for CMake target namespace) | `fmt` |
| `git_url` | HTTPS or SSH Git URL ending in `.git` | `https://github.com/fmtlib/fmt.git` |
| `git_tag` | Branch, tag, or commit SHA to fetch | `11.0.2`, `main`, `v3.4.0` |

---

## Quick Start

### Interactive Mode

```bash
cd my-project
sniffercommit init
```

When prompted for CMake options, answer `y` to **add dependencies**:

```
  add dependencies [n]: y

  enter dependency info (empty name to finish)

  dep name []: fmt
  git url [https://github.com/fmt/fmt.git]: https://github.com/fmtlib/fmt.git
  git tag [main]: 11.0.2
    ✓ added fmt

  dep name []: tomlplusplus
  git url [https://github.com/tomlplusplus/tomlplusplus.git]: 
  git tag [main]: v3.4.0
    ✓ added tomlplusplus

  dep name []: 
```

Press **Enter** at `dep name` with no input to finish.

### Non-Interactive (CLI)

```bash
sniffercommit init --enable-cmake \
  --add-dep fmt:https://github.com/fmtlib/fmt.git:11.0.2 \
  --add-dep tomlplusplus:https://github.com/marzer/tomlplusplus.git:v3.4.0
```

The `--add-dep` format is:

```
--add-dep <name>:<git_url>:<git_tag>
```

The `git_tag` is optional — if omitted, it defaults to `main`:

```bash
sniffercommit init --enable-cmake \
  --add-dep fmt:https://github.com/fmtlib/fmt.git
```

---

## Validation

Before any file is created, sniffercommit validates every dependency:

| Check | Error if failed |
|-------|-----------------|
| Name is non-empty | `Dependency name cannot be empty` |
| URL is non-empty | ``Dependency `fmt` missing git_url`` |
| URL format is valid | ``Dependency `fmt` has invalid git_url: bad-url`` |
| Tag is non-empty | ``Dependency `fmt` missing git_tag`` |

**Valid URL formats:**

```
https://github.com/user/repo.git
https://gitlab.com/user/repo.git
https://bitbucket.org/user/repo.git
git@github.com:user/repo.git
```

**Invalid URLs (rejected):**

```
not-a-url.git           # missing scheme
https://github.com/repo  # missing .git suffix
https://github.com/       # missing user/repo
```

---

## Generated CMakeLists.txt

For each validated dependency, sniffercommit generates:

```cmake
include(FetchContent)

FetchContent_Declare(
  fmt
  GIT_REPOSITORY https://github.com/fmtlib/fmt.git
  GIT_TAG 11.0.2
  GIT_SHALLOW ON
)

FetchContent_Declare(
  tomlplusplus
  GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
  GIT_TAG v3.4.0
  GIT_SHALLOW ON
)

FetchContent_MakeAvailable(fmt tomlplusplus)
```

And links them to your target:

```cmake
target_link_libraries(my-project PUBLIC
  fmt::fmt
  tomlplusplus::tomlplusplus
)
```

---

## Target Linking Convention

sniffercommit assumes dependencies follow the standard CMake namespace convention:

```cmake
target_link_libraries(<your-target> PUBLIC
  <name>::<name>
)
```

If a library uses a different namespace (e.g., `spdlog::spdlog_header_only`), you may need to adjust the generated `CMakeLists.txt` manually after init.

---

## Dependency Strategy

Currently, sniffercommit only supports **FetchContent** for dependency management. The `CMakeConfig` struct reserves `DepedencyStrategy` for future expansion:

| Strategy | Status | Description |
|----------|--------|-------------|
| `FetchContent` | Supported | CMake-native, no external tools |
| `FindPackage` | TODO | System packages via `find_package()` |
| `Conan` | TODO | Conan package manager integration |
| `Vcpkg` | TODO | Microsoft vcpkg toolchain |

---

## Examples

### fmt + tomlplusplus (common stack)

```bash
sniffercommit init --enable-cmake \
  --add-dep fmt:https://github.com/fmtlib/fmt.git:11.0.2 \
  --add-dep tomlplusplus:https://github.com/marzer/tomlplusplus.git:v3.4.0
```

### Google Test for testing

```bash
sniffercommit init --enable-cmake --cmake-enable-testing \
  --add-dep googletest:https://github.com/google/googletest.git:v1.14.0
```

### spdlog (header-only friendly)

```bash
sniffercommit init --enable-cmake --cmake-target-type header-only \
  --add-dep spdlog:https://github.com/gabime/spdlog.git:v1.13.0
```

---

## Troubleshooting

| Symptom | Cause | Fix |
|---------|-------|-----|
| `[ERROR] Dependency 'x' has invalid git_url` | URL missing `https://` or `.git` | Use full Git URL |
| `[ERROR] Dependency 'x' missing git_tag` | Tag field empty | Provide branch/tag/commit |
| CMake configure fails with "FetchContent_Declare not found" | CMake < 3.11 | Upgrade to CMake ≥ 3.20 |
| `target_link_libraries` fails | Wrong namespace | Edit `CMakeLists.txt` to match library's exported target |
| Build fails after `FetchContent_MakeAvailable` | Network unreachable / private repo | Check connectivity or use SSH key auth |

---

## Notes

- **No network check at init time**: sniffercommit validates URL *format*, not reachability. The actual fetch happens at CMake configure time.
- **GIT_SHALLOW ON**: All FetchContent declarations use shallow clones for faster downloads.
- **Order matters**: Dependencies are declared in the order you specify them. If `A` depends on `B`, declare `B` first.
- **Manual edits welcome**: After `init`, you can freely edit `CMakeLists.txt` to add `FIND_PACKAGE_ARGS`, `CMAKE_ARGS`, or switch to `ExternalProject`.
