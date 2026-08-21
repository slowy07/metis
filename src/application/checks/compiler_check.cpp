#include "metis/application/checks/compiler_check.hpp"

#include <fmt/format.h>

#include <algorithm>
#include <string>
#include <vector>

#include "metis/domain/ports/shell_executor.hpp"

namespace metis::application::checks {

CompilerCheck::CompilerCheck(const domain::config::Check& config)
    : domain::Check(config.name, config.description, config.enabled, config.patterns,
                    config.command, config.args, config.timeout, config.severity) {
  // If the user already specified a compilation mode (-c, -S, -E, -fsyntax-only),
  // respect it. Otherwise default to syntax-only (safe for pre-commit hooks).
  bool has_mode = std::ranges::any_of(arguments_, [](const std::string& arg) {
    return arg == "-c" || arg == "-S" || arg == "-E" || arg == "-fsyntax-only";
  });
  if (!has_mode) {
    arguments_.push_back("-fsyntax-only");
  }
}

domain::CheckResult CompilerCheck::execute(const std::vector<std::string>& files,
                                           domain::ports::IShellExecutor* shell, bool verbose,
                                           bool dry_run) {
  if (!shell->command_exists(command_)) {
    return {.exit_code = 1,
            .output = fmt::format("`{}` not found in PATH. install it or check your configuration",
                                  command_)};
  }

  if (dry_run) {
    return {.exit_code = 0, .output = {}};
  }

  // Compilers take one translation unit per invocation; batch is a non-goal.
  int overall_exit = 0;
  std::string accumulated_output;
  std::string verbose_log;

  for (const auto& file : files) {
    std::string full_cmd = command_line({file});

    if (verbose) {
      verbose_log += fmt::format("$ {}\n", full_cmd);
    }

    auto res = shell->exec_captured(full_cmd);
    if (res.exit_code_ != 0) {
      overall_exit = res.exit_code_;

      if (!res.output_.empty()) {
        accumulated_output += res.output_;
        if (!accumulated_output.empty() && accumulated_output.back() != '\n') {
          accumulated_output += '\n';
        }
      }
    }
  }

  return {.exit_code = overall_exit, .output = verbose_log + accumulated_output};
}

}  // namespace metis::application::checks
