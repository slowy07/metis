#ifndef SNIFFERCOMMIT_APPLICATION_CHECKS_COMPILER_CHECK_HPP
#define SNIFFERCOMMIT_APPLICATION_CHECKS_COMPILER_CHECK_HPP

#include "sniffercommit/domain/check.hpp"
#include "sniffercommit/domain/config.hpp"

namespace sniffercommit::application::checks {

// Compiler syntax check (GCC / Clang). Compiles each C/C++ file with
// `-fsyntax-only` so errors surface without producing object files.
class CompilerCheck : public domain::Check {
 public:
  explicit CompilerCheck(const domain::config::Check& config);

  [[nodiscard]] domain::CheckResult execute(const std::vector<std::string>& files,
                                            domain::ports::IShellExecutor* shell, bool verbose,
                                            bool dry_run) override;
};
}  // namespace sniffercommit::application::checks

#endif  // !SNIFFERCOMMIT_APPLICATION_CHECKS_COMPILER_CHECK_HPP
