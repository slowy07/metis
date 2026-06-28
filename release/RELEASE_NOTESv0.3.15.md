## v0.3.15 — Hexagonal Architecture Refactor + Windows Portability

This release completes a major internal redesign from monolithic code to a **hexagonal + layered architecture**, and adds full **Windows support** (both MSVC/`cmd.exe` and MinGW/MSYS2).

### What's Changed

- **No more `fork`/`exec`/`pipe`/`waitpid`** — The last inline POSIX process-spawning code has been replaced with a clean `IShellExecutor` port interface. The application layer is now fully portable.
- **Windows support** — Works natively on Windows with MSVC (`cmd.exe`) and MinGW/MSYS2. ANSI escape sequences are enabled automatically. Cross-compilation via MinGW on Linux is also supported.
- **Zero shell-escaping** — All git commands now use `std::filesystem::current_path()` (`chdir`) instead of `cd <path> && git ...`, eliminating platform shell quoting issues entirely.
- **Smart `create_directories`** — `OsFileSystem` now returns `true` when a directory already exists (not an error), matching user expectations.
- **All 19 tests pass** on Linux.

### Download

| Archive | Platform | Size |
|---------|----------|------|
| [sniffercommit-0.3.15-linux-x86_64.tar.gz](https://github.com/slowy07/sniffercommit/releases/download/v0.3.15/sniffercommit-0.3.15-linux-x86_64.tar.gz) | Linux (x86_64) | 1.3M |
| [sniffercommit-0.3.15-windows-x86_64.zip](https://github.com/slowy07/sniffercommit/releases/download/v0.3.15/sniffercommit-0.3.15-windows-x86_64.zip) | Windows (x86_64) | 1.7M |

### Checksums (SHA256)

```
e1e76353b085339e0802b12582d2748f6d5e41a1b29cb8012661fd137ba08d1a  sniffercommit-0.3.15-linux-x86_64
b1df67ccde736f54d953485b0c0edc3d7da7d815b6debefe51d809d00ec37776  sniffercommit-0.3.15-windows-x86_64.exe
```

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
