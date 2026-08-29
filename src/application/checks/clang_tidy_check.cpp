#include "metis/application/checks/clang_tidy_check.hpp"

#include <fmt/format.h>

#include <algorithm>
#include <filesystem>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "metis/domain/ports/shell_executor.hpp"
#include "metis/util.hpp"

namespace metis::application::checks {
namespace {
std::optional<std::string> extract_config_path(std::string_view arg, std::string_view prefix) {
  if (arg.starts_with(prefix)) {
    return std::string(arg.substr(prefix.length()));
  }

  return std::nullopt;
}
}  // namespace

ClangTidyCheck::ClangTidyCheck(const domain::config::Check& config)
  : domain::Check(config.name, config.description, config.enabled, config.patterns, config.command,
                  config.args, config.timeout, config.severity) {}

std::string ClangTidyCheck::validate(const std::filesystem::path& repo_root) const {
  bool has_explicit = false;
  for (const auto& arg : arguments()) {
    if (auto path = extract_config_path(arg, "--config-file=")) {
      has_explicit = true;
      auto config_path = std::filesystem::path(*path);
      if (config_path.is_relative()) {
        config_path = repo_root / config_path;
      }
      if (!std::filesystem::exists(config_path)) {
        return fmt::format(
            "Config file not found for `{}`: {}\n"
            " Run `metis init --enable-clang-tidy` to generate it.",
            name(), config_path.string());
      }
    }
  }

  if (!has_explicit) {
    auto default_path = repo_root / ".clang-tidy";
    if (!std::filesystem::exists(default_path)) {
      return fmt::format(
          "Default config file not found for `{}`: {}\n"
          " Run `metis init` to generate it.",
          name(), default_path.string());
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

  // clang-tidy wants: <tidy options> <sources...> [-- <compile flags>].
  // Config args may carry a bare "--" separating tidy options from compile
  // flags; the sources must go between them.
  std::vector<std::string> tidy_args;
  std::vector<std::string> compile_flags;
  if (auto sep = std::ranges::find(arguments(), "--"); sep != arguments().end()) {
    tidy_args.assign(arguments().begin(), sep);
    compile_flags.assign(std::next(sep), arguments().end());
  } else {
    tidy_args = arguments();
  }

  std::string full_cmd{command()};
  for (const auto& arg : tidy_args) {
    full_cmd += fmt::format(" {}", util::shell_escape(arg));
  }
  for (const auto& file : files) {
    full_cmd += fmt::format(" {}", util::shell_escape(file));
  }
  if (!compile_flags.empty()) {
    full_cmd += " --";
    for (const auto& flag : compile_flags) {
      full_cmd += fmt::format(" {}", util::shell_escape(flag));
    }
  }

  std::string output;
  if (verbose) {
    output += fmt::format("$ {}\n", full_cmd);
  }

  auto result = shell->exec_captured(full_cmd);
  return {.exit_code = result.exit_code_, .output = output + result.output_};
}

}  // namespace metis::application::checks
