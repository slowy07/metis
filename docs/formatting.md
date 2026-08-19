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

The generated pre-commit hook does not support `--format` directly. To auto-format on every commit, configure the hook to run format mode:

```bash
sniffercommit install

# The hook runs checks by default. To run format mode instead,
# edit .git/hooks/pre-commit to use:
sniffercommit run --format --staged
```
