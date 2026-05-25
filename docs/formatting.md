# Sniffercommit format Mode

Sniffercommit can run `--format` using clang-format in-place code formatting

run `clang-format` in-place on your files without running the full check suite

```bash
# format staged files
sniffercommit run --format

# format all traced files
sniffercommit run --format --all-files

# format specific files
sniffercommit run --format src/main.cpp include/foo.hpp

# dry-run
sniffercommit run --format --dry-run --all-files
```

requirements:
 - `clang-format`: must be installed and available in `PATH`
 - `.clang-format` config file must be exists (generated via `sniffercommit init`)


## Format in `Pre-Commit` Hook

to auto-format on every commit, pass `--format` to check:

```bash
sniffercommit install

# config git hook to use
.git/hooks/pre-commit --format
```
