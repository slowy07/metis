## v0.3.26 — Toolchain Install + Conan + Docs

### What's Changed

- **`install-compiler` subcommand** — installs a C/C++ compiler (GCC on POSIX, MinGW-w64 / LLVM-Clang on Windows) from official releases, with C++ standard selection (`--cpp-standard 17|20|23`), `--prefix`, `--force`, and `--dry-run`.
- **Toolchain infrastructure** — `IToolchainProvider` adapters (`PosixToolchainProvider`, `WindowsGccProvider`, `WindowsClangProvider`), `CurlHttpClient`, and tar/zip `IArchiveExtractor`s.
- **Conan support** — `sniffercommit init --enable-conan` generates a `conanfile.py` alongside CMake scaffolding.
- **Own pre-commit now runs clang-tidy** — `.sniffercommit.toml` gained a `clang-tidy` check; `tests/.clang-tidy` keeps test sources on a more permissive preset.
- **Refactored** `argparse` into a public header + implementation; removed dead code and duplicated helpers.
- **Docs** — added `docs/usage.md` and `docs/dev.md`; generated `.clang-format` defaults to `Standard: Cpp20`.

### Install

**Linux:**
```sh
curl -LsSf https://raw.githubusercontent.com/slowy07/sniffercommit/main/install.sh | sh
```

**Windows (Powershell):**
```powershell
powershell -ExecutionPolicy ByPass -c "irm https://raw.githubusercontent.com/slowy07/sniffercommit/main/install.ps1 | iex"
```

**Arch Linux (AUR):**
```sh
yay -S sniffercommit
```
