## v0.3.20 — Dead Code Removal + Over-Engineering Cleanup

This release removes ~3,100 lines of dead code from the old implementation, deduplicates helpers in the check execution path, and fixes the installer's source-build logic.

### What's Changed

- **Deleted 7 dead source pairs** — `tooling_config`, `executor`, `config_manager`, `project_config`, `precommit_domain`, `cicd_domain`, `error_codes.hpp` were superseded by the hexagonal architecture layer and never called.
- **Deleted unused `ILogger` port** — Interface had zero implementations; `Logger` singleton is used directly everywhere.
- **Deduplicated `run_checks_use_case.cpp`** — Removed local copies of `CwdGuard`, `shell_escape`, `check_command_exists`, and `Spinner` in favor of the shared utilities in `util.hpp` and `spinner.hpp`.
- **Cleaned up CMakeLists.txt** — Removed unused `SNIFFERCOMMIT_ENABLE_STATIC_LINK` option and dead libc detection block.
- **Fixed `install.sh`** — Replaced `cargo build` (copy-pasted from a Rust installer) with `cmake --build`. Removed stale `CARGO_HOME` references.
- **Cleaned up `config.hpp.in`** — Removed unused `BuildInfo::print()` and `#include <ostream>`.
- **Fixed clang-tidy warnings** — Initialized struct members, renamed short variables, added trailing `_` suffix to struct members per `.clang-tidy` config. Suppressed `bugprone-easily-swappable-parameters` and `cppcoreguidelines-init-variables` (too noisy for this codebase).
- **All 19 tests pass.**

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
