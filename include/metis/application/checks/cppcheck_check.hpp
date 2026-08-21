#ifndef METIS_APPLICATION_CHECKS_CPPCHECK_CHECK_CPP
#define METIS_APPLICATION_CHECKS_CPPCHECK_CHECK_CPP

#include "metis/domain/check.hpp"
#include "metis/domain/config.hpp"
#include "metis/domain/ports/shell_executor.hpp"

namespace metis::application::checks {

class CppcheckCheck : public domain::Check {
 public:
  explicit CppcheckCheck(const domain::config::Check& config);
  [[nodiscard]] domain::CheckResult execute(const std::vector<std::string>& files,
                                            domain::ports::IShellExecutor* shell, bool verbose,
                                            bool dry_run) override;
};

}  // namespace metis::application::checks

#endif  // !METIS_APPLICATION_CHECKS_CPPCHECK_CHECK_CPP
