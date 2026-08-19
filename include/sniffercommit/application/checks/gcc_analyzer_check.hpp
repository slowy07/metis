#ifndef SNIFFERCOMMIT_APPLICATION_CHECKS_GCC_ANALYZER_CHECK_HPP
#define SNIFFERCOMMIT_APPLICATION_CHECKS_GCC_ANALYZER_CHECK_HPP

#include "sniffercommit/domain/check.hpp"
#include "sniffercommit/domain/config.hpp"

namespace sniffercommit::application::checks {
class GCCAnalyzerCheck : public domain::Check {
 public:
  explicit GCCAnalyzerCheck(const domain::config::Check& config);
  [[nodiscard]] domain::CheckResult execute(const std::vector<std::string>& files,
                                            domain::ports::IShellExecutor* shell, bool verbose,
                                            bool dry_run) override;
};
}  // namespace sniffercommit::application::checks

#endif  // !SNIFFERCOMMIT_APPLICATION_CHECKS_GCC_ANALYZER_CHECK_HPP
