#include "sniffercommit/application/checks/shell_check.hpp"

#include <fmt/format.h>

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "sniffercommit/domain/ports/shell_executor.hpp"
#include "sniffercommit/util.hpp"

namespace sniffercommit::application::checks {
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
    : domain::Check(config.name, config.description, config.enabled, config.patterns,
                    config.command, config.args, config.timeout, config.severity) {
  auto basename = std::filesystem::path(command_).filename().string();
  if (basename == "grep" || basename == "egrep" || basename == "rg") {
    invert_exit_code_ = true;
  }
}

domain::CheckResult ShellCheck::execute(const std::vector<std::string>& files,
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
               k_multi_file_tools.count(std::filesystem::path(command_).filename().string());

  if (batch) {
    std::string full_cmd = cmd_base;

    for (const auto& file : files) {
      full_cmd += " " + util::shell_escape(file);
    }

    if (verbose) {
      verbose_log += fmt::format("$ {}\n", full_cmd);
    }

    auto result = shell->exec_captured(full_cmd);
    int code =
        invert_exit_code_ ? interpret_exit_code(result.exit_code_, command_) : result.exit_code_;

    if (code != 0) {
      overall_exit = code;
      accumulated_output = result.output_;
    }
  } else {
    for (const auto& file : files) {
      std::string full_cmd = fmt::format("{} {}", cmd_base, util::shell_escape(file));

      if (verbose) {
        verbose_log += fmt::format("$ {}\n", full_cmd);
      }

      auto res = shell->exec_captured(full_cmd);
      auto code =
          invert_exit_code_ ? interpret_exit_code(res.exit_code_, command_) : res.exit_code_;

      if (code != 0) {
        overall_exit = code;

        if (!res.output_.empty()) {
          accumulated_output += res.output_;

          if (!accumulated_output.empty() && accumulated_output.back() != '\n') {
            accumulated_output += '\n';
          }
        }
      }
    }
  }

  return {.exit_code = overall_exit, .output = verbose_log + accumulated_output};
}

}  // namespace sniffercommit::application::checks
