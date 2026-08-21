#ifndef METIS_APPLICATION_CHECKS_CLANG_TIDY_CHECK_HPP
#define METIS_APPLICATION_CHECKS_CLANG_TIDY_CHECK_HPP

#include "metis/domain/check.hpp"
#include "metis/domain/config.hpp"

namespace metis::application::checks {

// clang-tidy static-analysis check. Validates that the .clang-tidy config
// (explicit or default) exists, then runs clang-tidy over all files in a
// single batch invocation.
class ClangTidyCheck : public domain::Check {
 public:
  explicit ClangTidyCheck(const domain::config::Check& config);

  [[nodiscard]] std::string validate(const std::filesystem::path& repo_root) const override;
  [[nodiscard]] domain::CheckResult execute(const std::vector<std::string>& files,
                                            domain::ports::IShellExecutor* shell, bool verbose,
                                            bool dry_run) override;
};
}  // namespace metis::application::checks

#endif  // !METIS_APPLICATION_CHECKS_CLANG_TIDY_CHECK_HPP
