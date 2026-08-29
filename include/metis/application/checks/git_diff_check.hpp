#ifndef METIS_APPLICATION_CHECKS_GIT_DIFF_CHECK_HPP
#define METIS_APPLICATION_CHECKS_GIT_DIFF_CHECK_HPP

#include "metis/domain/check.hpp"
#include "metis/domain/config.hpp"

namespace metis::application::checks {

// Git diff check (e.g. `git diff --check` for whitespace errors). Runs the
// command once against the working tree, ignoring the matched file list.
class GitDiffCheck : public domain::Check {
 public:
  explicit GitDiffCheck(const domain::config::Check& config);

  [[nodiscard]] domain::CheckResult execute(const std::vector<std::string>& files,
                                            domain::ports::IShellExecutor* shell, bool verbose,
                                            bool dry_run) override;
};
}  // namespace metis::application::checks

#endif  // !METIS_APPLICATION_CHECKS_GIT_DIFF_CHECK_HPP
