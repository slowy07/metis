#ifndef SNIFFERCOMMIT_APPLICATION_CHECKS_IWYU_CHECK_HPP
#define SNIFFERCOMMIT_APPLICATION_CHECKS_IWYU_CHECK_HPP

#include <string>
#include <vector>

#include "sniffercommit/domain/check.hpp"
#include "sniffercommit/domain/config.hpp"
#include "sniffercommit/domain/ports/shell_executor.hpp"

namespace sniffercommit::application::checks {
class IWYUCheck : public domain::Check {
 public:
  explicit IWYUCheck(const domain::config::Check& config);

  [[nodiscard]] domain::CheckResult execute(const std::vector<std::string>& files,
                                            domain::ports::IShellExecutor* shell, bool verbose,
                                            bool dry_run) override;
};
}  // namespace sniffercommit::application::checks

#endif  // !SNIFFERCOMMIT_APPLICATION_CHECKS_IWYU_CHECK_HPP
