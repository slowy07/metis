#ifndef METIS_APPLICATION_CHECKS_SHELL_CHECK_APP
#define METIS_APPLICATION_CHECKS_SHELL_CHECK_APP

#include "metis/domain/check.hpp"
#include "metis/domain/config.hpp"

namespace metis::application::checks {

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
}  // namespace metis::application::checks

#endif  // !METIS_APPLICATION_CHECKS_SHELL_CHECK_APP
