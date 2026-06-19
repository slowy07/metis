#ifndef SNIFFERCOMMIT_DOMAIN_PORTS_SHELL_EXECUTOR_HPP
#define SNIFFERCOMMIT_DOMAIN_PORTS_SHELL_EXECUTOR_HPP

#include <string>

namespace sniffercommit::domain::ports {

struct CapturedResult {
  int exit_code;
  std::string output;
};

struct IShellExecutor {
  virtual ~IShellExecutor() = default;

  [[nodiscard]] virtual std::string exec(const std::string& cmd) = 0;
  [[nodiscard]] virtual CapturedResult exec_captured(const std::string& cmd) = 0;
  [[nodiscard]] virtual bool command_exists(const std::string& cmd) = 0;
};

}  // namespace sniffercommit::domain::ports

#endif
