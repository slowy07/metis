#include "sniffercommit/application/checks/clang_tidy_check.hpp"

#include <fmt/format.h>

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "sniffercommit/domain/ports/shell_executor.hpp"

namespace sniffercommit::application::checks {
namespace {
std::optional<std::string> extract_config_path(std::string_view arg, std::string_view prefix) {
  if (arg.starts_with(prefix)) {
    return std::string(arg.substr(prefix.length()));
  }

  return std::nullopt;
}
}  // namespace

ClangTidyCheck::ClangTidyCheck(const domain::config::Check& config)
    : domain::Check(config.name, config.description, config.enabled, config.patterns,
                    config.command, config.args, config.timeout, config.severity) {}

std::string ClangTidyCheck::validate(const std::filesystem::path& repo_root) const {
  bool has_explicit = false;
  for (const auto& arg : arguments_) {
    if (auto path = extract_config_path(arg, "--config-file=")) {
      has_explicit = true;
      auto config_path = std::filesystem::path(*path);
      if (config_path.is_relative()) {
        config_path = repo_root / config_path;
      }
      if (!std::filesystem::exists(config_path)) {
        return fmt::format(
            "Config file not found for `{}`: {}\n"
            " Run `sniffercommit init --enable-clang-tidy` to generate it.",
            name_, config_path.string());
      }
    }
  }

  if (!has_explicit) {
    auto default_path = repo_root / ".clang-tidy";
    if (!std::filesystem::exists(default_path)) {
      return fmt::format(
          "Default config file not found for `{}`: {}\n"
          " Run `sniffercommit init` to generate it.",
          name_, default_path.string());
    }
  }
  return "";
}

domain::CheckResult ClangTidyCheck::execute(const std::vector<std::string>& files,
                                            domain::ports::IShellExecutor* shell, bool verbose,
                                            bool dry_run) {
  if (!shell->command_exists("clang-tidy")) {
    return {.exit_code = 1,
            .output = "'clang-tidy' not found in PATH. install it or check your configuration"};
  }

  if (dry_run) {
    return {.exit_code = 0, .output = {}};
  }

  std::string full_cmd = command_line(files);

  std::string output;
  if (verbose) {
    output += fmt::format("$ {}\n", full_cmd);
  }

  auto result = shell->exec_captured(full_cmd);
  return {.exit_code = result.exit_code_, .output = output + result.output_};
}

}  // namespace sniffercommit::application::checks
