#ifndef METIS_APPLICATION_CHECKS_GCC_ANALYZER_CHECK_HPP
#define METIS_APPLICATION_CHECKS_GCC_ANALYZER_CHECK_HPP

#include "metis/domain/check.hpp"
#include "metis/domain/config.hpp"

namespace metis::application::checks {
class GCCAnalyzerCheck : public domain::Check {
 public:
  explicit GCCAnalyzerCheck(const domain::config::Check& config);
  [[nodiscard]] domain::CheckResult execute(const std::vector<std::string>& files,
                                            domain::ports::IShellExecutor* shell, bool verbose,
                                            bool dry_run) override;
};
}  // namespace metis::application::checks

#endif  // !METIS_APPLICATION_CHECKS_GCC_ANALYZER_CHECK_HPP
