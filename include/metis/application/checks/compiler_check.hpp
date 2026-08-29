#ifndef METIS_APPLICATION_CHECKS_COMPILER_CHECK_HPP
#define METIS_APPLICATION_CHECKS_COMPILER_CHECK_HPP

#include "metis/domain/check.hpp"
#include "metis/domain/config.hpp"

namespace metis::application::checks {

// Compiler syntax check (GCC / Clang). Compiles each C/C++ file with
// `-fsyntax-only` so errors surface without producing object files.
class CompilerCheck : public domain::Check {
 public:
  explicit CompilerCheck(const domain::config::Check& config);

  [[nodiscard]] domain::CheckResult execute(const std::vector<std::string>& files,
                                            domain::ports::IShellExecutor* shell, bool verbose,
                                            bool dry_run) override;
};
}  // namespace metis::application::checks

#endif  // !METIS_APPLICATION_CHECKS_COMPILER_CHECK_HPP
