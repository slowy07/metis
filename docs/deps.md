# Dependency Management

metis supports adding external C++ dependencies to your project via CMake [FetchContent](https://cmake.org/cmake/help/latest/module/FetchContent.html). Dependencies are declared during `metis init` and validated before any files are created.

---

## Overview

When you enable CMake scaffolding (`--enable-cmake` or interactive mode), metis can automatically generate `FetchContent_Declare` blocks in your `CMakeLists.txt`. Each dependency requires:

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
metis init
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
metis init --enable-cmake \
  --add-dep fmt:https://github.com/fmtlib/fmt.git:11.0.2 \
  --add-dep tomlplusplus:https://github.com/marzer/tomlplusplus.git:v3.4.0
```

The `--add-dep` format is:

```
--add-dep <name>:<git_url>:<git_tag>
```

The `git_tag` is optional: if omitted, it defaults to `main`:

```bash
metis init --enable-cmake \
  --add-dep fmt:https://github.com/fmtlib/fmt.git
```

---

## Validation

metis performs **no format validation** on `--add-dep` entries at init time.
The CMake generator splits each entry on the first and last colon; an entry
without any colon is skipped silently. A missing tag falls back to `main`.

Use `metis deps` to validate resolved dependencies (versions, local
availability, lockfiles, duplicates).

---

## Generated CMakeLists.txt

For each validated dependency, metis generates:

```cmake
include(FetchContent)

FetchContent_Declare(
  fmt
  GIT_REPOSITORY https://github.com/fmtlib/fmt.git
  GIT_TAG 11.0.2
)

FetchContent_Declare(
  tomlplusplus
  GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
  GIT_TAG v3.4.0
)
```

`FetchContent_MakeAvailable(...)` and `target_link_libraries(...)` lines are
not generated — add them to suit your target setup.

With `--enable-conan`, `find_package(<name> REQUIRED)` is emitted instead of
FetchContent blocks, and a `conanfile.py` is generated.

---

## Target Linking Convention

metis assumes dependencies follow the standard CMake namespace convention:

```cmake
target_link_libraries(<your-target> PUBLIC
  <name>::<name>
)
```

If a library uses a different namespace (e.g., `spdlog::spdlog_header_only`), you may need to adjust the generated `CMakeLists.txt` manually after init.

---

## Dependency Strategy

Currently, metis only supports **FetchContent** for dependency management. The `CMakeConfig` struct reserves `DepedencyStrategy` for future expansion:

| Strategy | Status | Description |
|----------|--------|-------------|
| `FetchContent` | Supported | CMake-native, no external tools |
| `Conan` | Supported (`--enable-conan`) | Generates `conanfile.py` + `find_package` calls |
| `Vcpkg` | TODO | Microsoft vcpkg toolchain |

---

## Examples

### fmt + tomlplusplus (common stack)

```bash
metis init --enable-cmake \
  --add-dep fmt:https://github.com/fmtlib/fmt.git:11.0.2 \
  --add-dep tomlplusplus:https://github.com/marzer/tomlplusplus.git:v3.4.0
```

### Google Test for testing

```bash
metis init --enable-cmake --cmake-enable-testing \
  --add-dep googletest:https://github.com/google/googletest.git:v1.14.0
```

### spdlog (header-only friendly)

```bash
metis init --enable-cmake --cmake-target-type header-only \
  --add-dep spdlog:https://github.com/gabime/spdlog.git:v1.13.0
```

---

## Troubleshooting

| Symptom | Cause | Fix |
|---------|-------|-----|
| `✖ <dep>  [missing version]` | Dependency declared without a version | Add a version to the dep |
| `✖ <dep>  [invalid semver: x]` | Version is not valid semver | Use e.g. `10.2.1` |
| `✖ <dep>  [not installed locally]` | Conan/vcpkg manifest lists it but it is not available to CMake | Run `conan install` / `vcpkg install` |
| CMake configure fails with "FetchContent_Declare not found" | CMake < 3.11 | Upgrade to CMake ≥ 3.20 |
| `target_link_libraries` fails | Wrong namespace | Edit `CMakeLists.txt` to match library's exported target |
| Build fails after `FetchContent_MakeAvailable` | Network unreachable / private repo | Check connectivity or use SSH key auth |

---

## Notes

- **No network check at init time**: metis checks neither format nor reachability; the fetch happens at CMake configure time.
- **Order matters**: Dependencies are declared in the order you specify them. If `A` depends on `B`, declare `B` first.
- **Manual edits welcome**: After `init`, you can freely edit `CMakeLists.txt` to add `FIND_PACKAGE_ARGS`, `CMAKE_ARGS`, or switch to `ExternalProject`.
