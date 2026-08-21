#include "metis/application/checks/clang_static_analyzer_check.hpp"

#include <fmt/format.h>

#include <string>
#include <vector>

#include "metis/domain/check.hpp"
#include "metis/domain/config.hpp"
#include "metis/domain/ports/shell_executor.hpp"

namespace metis::application::checks {

ClangStaticAnalyzerCheck::ClangStaticAnalyzerCheck(const domain::config::Check& config)
    : domain::Check(config.name, config.description, config.enabled, config.patterns,
                    config.command, config.args, config.timeout, config.severity) {}

domain::CheckResult ClangStaticAnalyzerCheck::execute(const std::vector<std::string>& /*files*/,
                                                      domain::ports::IShellExecutor* shell,
                                                      bool verbose, bool dry_run) {
  if (!shell->command_exists(command_)) {
    return {.exit_code = 1,
            .output = fmt::format("`{}` not found in PATH. install it or check your configuration",
                                  command_)};
  }

  if (dry_run) {
    return {.exit_code = 0, .output = ""};
  }

  std::string full_cmd = command_;
  for (const auto& arguments : arguments_) {
    full_cmd += " " + arguments;
  }

  if (arguments_.empty()) {
    full_cmd += " --status-bugs";
  }

  if (verbose) {
    full_cmd += " -V";
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
