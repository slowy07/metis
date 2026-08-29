#ifndef METIS_CHECKS_CLANG_STATIC_ANALYZER_CHECK_HPP
#define METIS_CHECKS_CLANG_STATIC_ANALYZER_CHECK_HPP

#include "metis/domain/check.hpp"
#include "metis/domain/config.hpp"

namespace metis::application::checks {

class ClangStaticAnalyzerCheck : public domain::Check {
 public:
  explicit ClangStaticAnalyzerCheck(const domain::config::Check& config);
  [[nodiscard]] domain::CheckResult execute(const std::vector<std::string>& files,
                                            domain::ports::IShellExecutor* shell, bool verbose,
                                            bool dry_run) override;
};
}  // namespace metis::application::checks

#endif  // !METIS_CHECKS_CLANG_STATIC_ANALYZER_CHECK_HPP
