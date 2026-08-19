#ifndef SNIFFERCOMMIT_CHECKS_CLANG_STATIC_ANALYZER_CHECK_HPP
#define SNIFFERCOMMIT_CHECKS_CLANG_STATIC_ANALYZER_CHECK_HPP

#include "sniffercommit/domain/check.hpp"
#include "sniffercommit/domain/config.hpp"

namespace sniffercommit::application::checks {

class ClangStaticAnalyzerCheck : public domain::Check {
 public:
  explicit ClangStaticAnalyzerCheck(const domain::config::Check& config);
  [[nodiscard]] domain::CheckResult execute(const std::vector<std::string>& files,
                                            domain::ports::IShellExecutor* shell, bool verbose,
                                            bool dry_run) override;
};
}  // namespace sniffercommit::application::checks

#endif  // !SNIFFERCOMMIT_CHECKS_CLANG_STATIC_ANALYZER_CHECK_HPP
