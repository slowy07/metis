#include "metis/application/checks/git_diff_check.hpp"

#include <fmt/format.h>

#include <string>
#include <vector>

#include "metis/domain/ports/shell_executor.hpp"

namespace metis::application::checks {

GitDiffCheck::GitDiffCheck(const domain::config::Check& config)
  : domain::Check(config.name, config.description, config.enabled, config.patterns, config.command,
                  config.args, config.timeout, config.severity) {}

domain::CheckResult GitDiffCheck::execute(const std::vector<std::string>& /*files*/,
                                          domain::ports::IShellExecutor* shell, bool verbose,
                                          bool dry_run) {
  if (!shell->command_exists(command())) {
    return {.exit_code = 1,
            .output = fmt::format("`{}` not found in PATH. install it or check your configuration",
                                  command())};
  }

  if (dry_run) {
    return {.exit_code = 0, .output = {}};
  }

  // `git diff` runs against the whole working tree; appending file args would
  // change its scope, so run it once with no file list.
  std::string full_cmd = command_line({});

  std::string output;
  if (verbose) {
    output += fmt::format("$ {}\n", full_cmd);
  }

  auto result = shell->exec_captured(full_cmd);
  return {.exit_code = result.exit_code_, .output = output + result.output_};
}

}  // namespace metis::application::checks
