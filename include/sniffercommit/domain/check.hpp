#ifndef SNIFFERCOMMIT_DOMAIN_CHECK_HPP
#define SNIFFERCOMMIT_DOMAIN_CHECK_HPP

#include <filesystem>
#include <string>
#include <vector>

namespace sniffercommit::domain {
namespace ports {
struct IShellExecutor;
}

// Result of running a check against a set of files.
struct CheckResult {
  int exit_code = 0;
  std::string output;
};

// Generic check abstraction. Every check carries its own config
// (name, description, enabled, file_patterns, command, arguments,
// timeout, severity) and implements execute().
class Check {
 public:
  Check(std::string name, std::string description, bool enabled,
        std::vector<std::string> file_patterns, std::string command,
        std::vector<std::string> arguments, int timeout, std::string severity);
  virtual ~Check() = default;

  [[nodiscard]] const std::string& name() const { return name_; }
  [[nodiscard]] const std::string& description() const { return description_; }
  [[nodiscard]] bool enabled() const { return enabled_; }
  [[nodiscard]] const std::vector<std::string>& file_patterns() const { return file_patterns_; }
  [[nodiscard]] const std::string& command() const { return command_; }
  [[nodiscard]] const std::vector<std::string>& arguments() const { return arguments_; }
  [[nodiscard]] int timeout() const { return timeout_; }
  [[nodiscard]] const std::string& severity() const { return severity_; }

  // Verifies the environment (tool config files, etc.). Empty string = OK.
  [[nodiscard]] virtual std::string validate(const std::filesystem::path& /*repo_root*/) const {
    return "";
  }

  [[nodiscard]] virtual CheckResult execute(const std::vector<std::string>& files,
                                            ports::IShellExecutor* shell, bool verbose,
                                            bool dry_run) = 0;

 protected:
  // Builds the escaped shell command: command + arguments + files.
  // All check types use the same shape, so command building lives here.
  [[nodiscard]] std::string command_line(const std::vector<std::string>& files) const;

  std::string name_;
  std::string description_;
  bool enabled_;
  std::vector<std::string> file_patterns_;
  std::string command_;
  std::vector<std::string> arguments_;
  int timeout_;
  std::string severity_;
};

}  // namespace sniffercommit::domain

#endif  // !SNIFFERCOMMIT_DOMAIN_CHECK_HPP
