---
name: Pull Request
about: Propose changes to sniffercommit
title: ""
labels: ''
assignees: ''

---

<!--
  Thank you for your contribution! Please follow this template to ensure
  efficient review. Incomplete PRs may be closed or returned for revision.
-->

## PR Checklist

> **Self-review before requesting review:**

- [ ] I have read the [CONTRIBUTING.md](../CONTRIBUTING.md) guidelines.
- [ ] My branch is up-to-date with the `develop` branch (`git rebase develop` or `git merge develop`).
- [ ] All tests pass locally (`ctest --test-dir build --output-on-failure`).
- [ ] New code has **>80% line coverage** (verified via `lcov` or CI report).
- [ ] Code is formatted with the project's `.clang-format` style.
- [ ] Static analysis passes (`clang-tidy` standard preset or higher).
- [ ] Commit messages follow [Conventional Commits](https://www.conventionalcommits.org/).
- [ ] No compiler warnings introduced (GCC/Clang `-Wall -Wextra -Wpedantic` clean).
- [ ] Sanitizer builds pass (`-DSNIFFERCOMMIT_ENABLE_SANITIZERS=ON`).
- [ ] Documentation updated (README, help text, design docs, or CHANGELOG).


### Test Coverage
File Lines Missed Cover
src/executor.cpp 150 12 92% src/config_manager.cpp 80 5 94%
TOTAL                       230      17       92.6%
plain
Copy

### New Tests Added

- `test_executor.cpp`: `ExecutorTest.FormatMode_DryRun_NoModification`
- `test_executor.cpp`: `ExecutorTest.FormatMode_ActualRun_ModifiesFiles`
- `test_config_manager.cpp`: `ConfigManagerTest.ValidateDuplicateCheckNames_Throws`

