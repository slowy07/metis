#ifndef SNIFFERCOMMIT_APPLICATION_CHECKS_CLANG_FORMAT_CHECK_HPP
#define SNIFFERCOMMIT_APPLICATION_CHECKS_CLANG_FORMAT_CHECK_HPP

#include "sniffercommit/domain/check.hpp"
#include "sniffercommit/domain/config.hpp"

namespace sniffercommit::application::checks {

// clang-format formatting check. Runs `clang-format -i` on C/C++ files,
// then reports which files were actually modified (via git diff).
class ClangFormatCheck : public domain::Check {
 public:
  explicit ClangFormatCheck(const domain::config::Check& config);

  [[nodiscard]] std::string validate(const std::filesystem::path& repo_root) const override;
  [[nodiscard]] domain::CheckResult execute(const std::vector<std::string>& files,
                                            domain::ports::IShellExecutor* shell, bool verbose,
                                            bool dry_run) override;
};
}  // namespace sniffercommit::application::checks

#endif  // !SNIFFERCOMMIT_APPLICATION_CHECKS_CLANG_FORMAT_CHECK_HPP
