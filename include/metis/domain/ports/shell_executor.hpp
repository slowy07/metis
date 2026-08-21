#ifndef METIS_DOMAIN_PORTS_SHELL_EXECUTOR_HPP
#define METIS_DOMAIN_PORTS_SHELL_EXECUTOR_HPP

#include <string>

namespace metis::domain::ports {

// Result of a command execution that captures both output and exit code.
struct CapturedResult {
  int exit_code_;
  std::string output_;
};

// Interface for executing shell commands.
// lazy: only one implementation (ProcessShellExecutor). Same reasoning
// as IConfigRepository — interface for potential test mocking.
struct IShellExecutor {
  virtual ~IShellExecutor() = default;

  [[nodiscard]] virtual std::string exec(const std::string& cmd) = 0;
  [[nodiscard]] virtual CapturedResult exec_captured(const std::string& cmd) = 0;
  [[nodiscard]] virtual bool command_exists(const std::string& cmd) = 0;
};

}  // namespace metis::domain::ports

#endif
