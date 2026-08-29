#ifndef METIS_INFRASTRUCTURE_PROCESS_SHELL_EXECUTOR_HPP
#define METIS_INFRASTRUCTURE_PROCESS_SHELL_EXECUTOR_HPP

#include "metis/domain/ports/shell_executor.hpp"

namespace metis::infrastructure {

class ProcessShellExecutor : public domain::ports::IShellExecutor {
 public:
  [[nodiscard]] std::string exec(const std::string& cmd) override;
  [[nodiscard]] domain::ports::CapturedResult exec_captured(const std::string& cmd) override;
  [[nodiscard]] bool command_exists(const std::string& cmd) override;
};

}  // namespace metis::infrastructure

#endif
