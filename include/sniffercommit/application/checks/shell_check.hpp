#ifndef SNIFFERCOMMIT_APPLICATION_CHECKS_SHELL_CHECK_APP
#define SNIFFERCOMMIT_APPLICATION_CHECKS_SHELL_CHECK_APP

#include "sniffercommit/domain/check.hpp"
#include "sniffercommit/domain/config.hpp"

namespace sniffercommit::application::checks {

class ShellCheck : public domain::ICheck {
 public:
  explicit ShellCheck(const domain::config::Check& config);

  [[nodiscard]] std::string name() const override;
  [[nodiscard]] std::string description() const override;
  [[nodiscard]] bool enabled() const override;
  [[nodiscard]] std::vector<std::string> file_patterns() const override;
  [[nodiscard]] int timeout() const override;
  [[nodiscard]] std::string severity() const override;
  [[nodiscard]] std::string validate(const std::filesystem::path& repo_root) const override;
  [[nodiscard]] domain::CheckResult execute(const std::vector<std::string>& files,
                                            domain::ports::IShellExecutor* shell, bool verbose,
                                            bool dry_run) override;

 private:
  std::string name_;
  std::string description_;
  bool enabled_;
  std::vector<std::string> patterns_;
  std::string command_;
  std::vector<std::string> args_;
  int timeout_;
  std::string severity_;
  bool invert_exit_code_;
};
}  // namespace sniffercommit::application::checks

#endif  // !SNIFFERCOMMIT_APPLICATION_CHECKS_SHELL_CHECK_APP
