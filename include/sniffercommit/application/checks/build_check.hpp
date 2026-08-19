#ifndef SNIFFERCOMMIT_APPLICATION_CHECKS_BUILD_CHECK_HPP
#define SNIFFERCOMMIT_APPLICATION_CHECKS_BUILD_CHECK_HPP

#include "sniffercommit/domain/check.hpp"
#include "sniffercommit/domain/config.hpp"

namespace sniffercommit::application::checks {

// Build check (e.g. `cmake --build build`). Runs the command once with its
// arguments and no file list; the exit code is the build result.
class BuildCheck : public domain::Check {
 public:
  explicit BuildCheck(const domain::config::Check& config);

  [[nodiscard]] domain::CheckResult execute(const std::vector<std::string>& files,
                                            domain::ports::IShellExecutor* shell, bool verbose,
                                            bool dry_run) override;
};
}  // namespace sniffercommit::application::checks

#endif  // !SNIFFERCOMMIT_APPLICATION_CHECKS_BUILD_CHECK_HPP
