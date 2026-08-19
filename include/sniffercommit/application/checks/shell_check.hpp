#ifndef SNIFFERCOMMIT_APPLICATION_CHECKS_SHELL_CHECK_APP
#define SNIFFERCOMMIT_APPLICATION_CHECKS_SHELL_CHECK_APP

#include "sniffercommit/domain/check.hpp"
#include "sniffercommit/domain/config.hpp"

namespace sniffercommit::application::checks {

// Custom-command check: runs `command arguments files...`.
// Covers arbitrary linters and file/pattern validation (grep/rg exit codes
// are inverted so "no match" = pass). Every command not claimed by a more
// specific check type routes here.
class ShellCheck : public domain::Check {
 public:
  explicit ShellCheck(const domain::config::Check& config);

  [[nodiscard]] domain::CheckResult execute(const std::vector<std::string>& files,
                                            domain::ports::IShellExecutor* shell, bool verbose,
                                            bool dry_run) override;

 private:
  bool invert_exit_code_ = false;
};
}  // namespace sniffercommit::application::checks

#endif  // !SNIFFERCOMMIT_APPLICATION_CHECKS_SHELL_CHECK_APP
