#include "metis/application/checks/shell_check.hpp"

#include <fmt/format.h>

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "metis/domain/ports/shell_executor.hpp"
#include "metis/util.hpp"

namespace metis::application::checks {
namespace {
int interpret_exit_code(int raw, std::string_view cmd) {
  auto basename = std::filesystem::path(cmd).filename().string();

  // grep/rg exit codes are inverted from the normal convention:
  //   exit 0 = match found (but for a pre-commit hook, match = problem)
  //   exit 1 = no match (clean, success)
  if ((basename == "grep" || basename == "egrep" || basename == "rg") && (raw == 0 || raw == 1)) {
    return raw == 0 ? 1 : 0;
  }

  return raw;
}
}  // namespace

ShellCheck::ShellCheck(const domain::config::Check& config)
  : domain::Check(config.name, config.description, config.enabled, config.patterns, config.command,
                  config.args, config.timeout, config.severity) {
  auto basename = std::filesystem::path(command()).filename().string();
  if (basename == "grep" || basename == "egrep" || basename == "rg") {
    invert_exit_code_ = true;
  }
}

// Appends the executed command line to the verbose log when enabled.
void log_command(std::string& verbose_log, const std::string& cmd, bool verbose) {
  if (verbose) {
    verbose_log += fmt::format("$ {}\n", cmd);
  }
}

// Single multi-file invocation for tools that accept several file arguments
// (faster: config/args parsed once). Records failure into overall/output.
void run_batch(domain::ports::IShellExecutor* shell, const std::string& cmd_base,
               const std::vector<std::string>& files, bool invert, const std::string& tool_name,
               bool verbose, std::string& verbose_log, int& overall_exit,
               std::string& accumulated_output) {
  std::string full_cmd = cmd_base;
  for (const auto& file : files) {
    full_cmd += " " + util::shell_escape(file);
  }
  log_command(verbose_log, full_cmd, verbose);

  auto result = shell->exec_captured(full_cmd);
  const int code = invert ? interpret_exit_code(result.exit_code_, tool_name) : result.exit_code_;
  if (code != 0) {
    overall_exit = code;
    accumulated_output = result.output_;
  }
}

// One invocation per file; appends failing output with newline normalization.
void run_per_file(domain::ports::IShellExecutor* shell, const std::string& cmd_base,
                  const std::vector<std::string>& files, bool invert, const std::string& tool_name,
                  bool verbose, std::string& verbose_log, int& overall_exit,
                  std::string& accumulated_output) {
  for (const auto& file : files) {
    std::string full_cmd = fmt::format("{} {}", cmd_base, util::shell_escape(file));
    log_command(verbose_log, full_cmd, verbose);

    auto res = shell->exec_captured(full_cmd);
    const int code = invert ? interpret_exit_code(res.exit_code_, tool_name) : res.exit_code_;
    if (code != 0) {
      overall_exit = code;
      if (!res.output_.empty()) {
        accumulated_output += res.output_;
        if (accumulated_output.back() != '\n') {
          accumulated_output += '\n';
        }
      }
    }
  }
}

domain::CheckResult ShellCheck::execute(const std::vector<std::string>& files,
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

  std::string cmd_base = command_line({});

  int overall_exit = 0;
  std::string accumulated_output;
  std::string verbose_log;

  // Tools that accept multiple file arguments in one invocation.
  // These are faster in batch mode because they only parse config/args once.
  static const std::unordered_set<std::string> k_multi_file_tools = {
      "clang-format", "clang-tidy", "grep", "egrep", "rg", "cppcheck",
  };
  bool batch = files.size() > 1 &&
               k_multi_file_tools.contains(std::filesystem::path(command()).filename().string());

  if (batch) {
    run_batch(shell, cmd_base, files, invert_exit_code_, command(), verbose, verbose_log,
              overall_exit, accumulated_output);
  } else {
    run_per_file(shell, cmd_base, files, invert_exit_code_, command(), verbose, verbose_log,
                 overall_exit, accumulated_output);
  }

  return {.exit_code = overall_exit, .output = verbose_log + accumulated_output};
}

}  // namespace metis::application::checks
