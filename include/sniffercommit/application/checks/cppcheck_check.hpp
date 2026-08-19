#ifndef SNIFFERCOMMIT_APPLICATION_CHECKS_CPPCHECK_CHECK_CPP
#define SNIFFERCOMMIT_APPLICATION_CHECKS_CPPCHECK_CHECK_CPP

#include "sniffercommit/domain/check.hpp"
#include "sniffercommit/domain/config.hpp"
#include "sniffercommit/domain/ports/shell_executor.hpp"

namespace sniffercommit::application::checks {

class CppcheckCheck : public domain::Check {
 public:
  explicit CppcheckCheck(const domain::config::Check& config);
  [[nodiscard]] domain::CheckResult execute(const std::vector<std::string>& files,
                                            domain::ports::IShellExecutor* shell, bool verbose,
                                            bool dry_run) override;
};

}  // namespace sniffercommit::application::checks

#endif  // !SNIFFERCOMMIT_APPLICATION_CHECKS_CPPCHECK_CHECK_CPP
