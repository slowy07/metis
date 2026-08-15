#ifndef SNIFFERCOMMIT_DOMAIN_CHECK_HPP
#define SNIFFERCOMMIT_DOMAIN_CHECK_HPP

#include <filesystem>
#include <string>
#include <vector>
namespace sniffercommit::domain {
namespace ports {
struct IShellExecutor;
}

struct CheckResult {
  int exit_code = 0;
  std::string output;
};

class ICheck {
 public:
  virtual ~ICheck() = default;

  [[nodiscard]] virtual std::string name() const = 0;
  [[nodiscard]] virtual std::string description() const = 0;
  [[nodiscard]] virtual bool enabled() const = 0;
  [[nodiscard]] virtual std::vector<std::string> file_patterns() const = 0;
  [[nodiscard]] virtual int timeout() const = 0;
  [[nodiscard]] virtual std::string severity() const = 0;
  [[nodiscard]] virtual std::string validate(const std::filesystem::path& repo_root) const = 0;

  [[nodiscard]] virtual CheckResult execute(const std::vector<std::string>& files,
                                            ports::IShellExecutor* shell, bool verbose,
                                            bool dry_run) = 0;
};
}  // namespace sniffercommit::domain

#endif  // !SNIFFERCOMMIT_DOMAIN_CHECK_HPP
