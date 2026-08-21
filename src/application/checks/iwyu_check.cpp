#include "metis/application/checks/iwyu_check.hpp"

#include <fmt/format.h>

#include <string>
#include <vector>

#include "metis/domain/check.hpp"
#include "metis/domain/config.hpp"
#include "metis/domain/ports/shell_executor.hpp"
#include "metis/util.hpp"

namespace metis::application::checks {
IWYUCheck::IWYUCheck(const domain::config::Check& config)
    : domain::Check(config.name, config.description, config.enabled, config.patterns,
                    config.command, config.args, config.timeout, config.severity) {}

domain::CheckResult IWYUCheck::execute(const std::vector<std::string>& files,
                                       domain::ports::IShellExecutor* shell, bool verbose,
                                       bool dry_run) {
  if (!shell->command_exists(command_)) {
    return {.exit_code = 1,
            .output = fmt::format("`{}` not found in PATH. install it or check your configuration",
                                  command_)};
  }

  if (dry_run) {
    return {.exit_code = 0, .output = ""};
  }

  std::string combined_output;
  int last_exit = 0;

  for (const auto& file : files) {
    std::string full_cmd = command_ + " " + util::shell_escape(file);
    for (const auto& arguments : arguments_) {
      full_cmd += " " + arguments;
    }

    if (verbose) {
      combined_output += fmt::format("$ {}\n", full_cmd);
    }

    auto result = shell->exec_captured(full_cmd);
    combined_output += result.output_;
    if (result.exit_code_ != 0) {
      last_exit = result.exit_code_;
    }
  }

  return {.exit_code = last_exit, .output = combined_output};
}
}  // namespace metis::application::checks
