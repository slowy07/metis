#include "metis/application/checks/cppcheck_check.hpp"

#include <fmt/format.h>

#include <string>
#include <vector>

#include "metis/domain/check.hpp"
#include "metis/domain/config.hpp"
#include "metis/domain/ports/shell_executor.hpp"
#include "metis/util.hpp"

namespace metis::application::checks {

CppcheckCheck::CppcheckCheck(const domain::config::Check& config)
  : domain::Check(config.name, config.description, config.enabled, config.patterns, config.command,
                  config.args, config.timeout, config.severity) {}

domain::CheckResult CppcheckCheck::execute(const std::vector<std::string>& files,
                                           domain::ports::IShellExecutor* shell, bool verbose,
                                           bool dry_run) {
  if (!shell->command_exists(command())) {
    return {.exit_code = 1,
            .output = fmt::format("`{}` not found in PATH. install it or check your configuration",
                                  command())};
  }

  if (dry_run) {
    return {.exit_code = 0, .output = ""};
  }

  std::string full_cmd = command_line(files);
  bool has_enable = false;

  for (const auto& arguments : arguments()) {
    if (arguments.find("--enable") != std::string::npos) {
      has_enable = true;
      break;
    }
  }

  if (has_enable) {
    full_cmd += " --enable=all";
  }

  bool has_suppression = false;

  for (const auto& arguments : arguments()) {
    if (arguments.find("--suppress") != std::string::npos ||
        arguments.find("--suppression-list=") != std::string::npos) {
      has_suppression = true;
      break;
    }
  }

  if (!has_suppression) {
    full_cmd += " --suppress=missingInclude";
  }

  std::string output;
  if (verbose) {
    output += fmt::format("$ {}\n", full_cmd);
  }

  auto result = shell->exec_captured(full_cmd);
  output += result.output_;

  return {.exit_code = result.exit_code_, .output = output};
}

}  // namespace metis::application::checks
