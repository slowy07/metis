#ifndef SNIFFERCOMMIT_INFRASTRUCTURE_PROCESS_SHELL_EXECUTOR_HPP
#define SNIFFERCOMMIT_INFRASTRUCTURE_PROCESS_SHELL_EXECUTOR_HPP

#include "sniffercommit/domain/ports/shell_executor.hpp"

namespace sniffercommit::infrastructure {

class ProcessShellExecutor : public domain::ports::IShellExecutor {
 public:
  [[nodiscard]] std::string exec(const std::string& cmd) override;
  [[nodiscard]] domain::ports::CapturedResult exec_captured(const std::string& cmd) override;
  [[nodiscard]] bool command_exists(const std::string& cmd) override;
};

}  // namespace sniffercommit::infrastructure

#endif
