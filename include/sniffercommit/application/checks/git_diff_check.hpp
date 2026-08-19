#ifndef SNIFFERCOMMIT_APPLICATION_CHECKS_GIT_DIFF_CHECK_HPP
#define SNIFFERCOMMIT_APPLICATION_CHECKS_GIT_DIFF_CHECK_HPP

#include "sniffercommit/domain/check.hpp"
#include "sniffercommit/domain/config.hpp"

namespace sniffercommit::application::checks {

// Git diff check (e.g. `git diff --check` for whitespace errors). Runs the
// command once against the working tree, ignoring the matched file list.
class GitDiffCheck : public domain::Check {
 public:
  explicit GitDiffCheck(const domain::config::Check& config);

  [[nodiscard]] domain::CheckResult execute(const std::vector<std::string>& files,
                                            domain::ports::IShellExecutor* shell, bool verbose,
                                            bool dry_run) override;
};
}  // namespace sniffercommit::application::checks

#endif  // !SNIFFERCOMMIT_APPLICATION_CHECKS_GIT_DIFF_CHECK_HPP
