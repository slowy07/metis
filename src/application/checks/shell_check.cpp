#include "sniffercommit/application/checks/shell_check.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>

#include "fmt/format.h"
#include "sniffercommit/domain/ports/shell_executor.hpp"
#include "sniffercommit/util.hpp"

namespace sniffercommit::application::checks {
namespace {
int interpret_exit_code(int raw, std::string_view cmd) {
  auto basename = std::filesystem::path(cmd).filename().string();

  if ((basename == "grep" || basename == "egrep" || basename == "rg") && (raw == 0 || raw == 1)) {
    return raw == 0 ? 1 : 0;
  }

  return raw;
}

std::optional<std::string> extract_config_path(std::string_view arg, std::string_view prefix) {
  if (arg.starts_with(prefix)) {
    return std::string(arg.substr(prefix.length()));
  }

  return std::nullopt;
}

struct ConfigRequirement {
  std::string tool_name;
  std::string config_arg;
  std::string default_file;
};

}  // namespace

ShellCheck::ShellCheck(const domain::config::Check& config)
    : name_(config.name),
      description_(config.description),
      enabled_(config.enabled),
      patterns_(config.patterns),
      command_(config.command),
      args_(config.args),
      timeout_(config.timeout),
      severity_(config.severity),
      invert_exit_code_(false) {
  auto basename = std::filesystem::path(command_).filename().string();
  if (basename == "grep" || basename == "egrep" || basename == "rg") {
    invert_exit_code_ = true;
  }
}

std::string ShellCheck::name() const { return name_; }
std::string ShellCheck::description() const { return description_; }
bool ShellCheck::enabled() const { return enabled_; }
std::vector<std::string> ShellCheck::file_patterns() const { return patterns_; }
int ShellCheck::timeout() const { return timeout_; }
std::string ShellCheck::severity() const { return severity_; }

std::string ShellCheck::validate(const std::filesystem::path& repo_root) const {
  static const std::vector<ConfigRequirement> k_config_tools = {
      {"clang-tidy", "--config-file=", ".clang-tidy"},
      {"clang-format", "", ".clang-format"},
  };

  for (const auto& req : k_config_tools) {
    if (command_ != req.tool_name) {
      continue;
    }

    bool has_explicit = false;

    for (const auto& arg : args_) {
      if (req.config_arg.empty()) {
        break;
      }
      if (auto path = extract_config_path(arg, req.config_arg)) {
        has_explicit = true;
        auto config_path = std::filesystem::path(*path);
        if (config_path.is_relative()) {
          config_path = repo_root / config_path;
        }
        if (!std::filesystem::exists(config_path)) {
          return fmt::format(
              "Config file not found for `{}`: {}\n"
              " Run `sniffercommit init --enable-clang-tidy` to generate it.\n"
              " or ensure the file exists at the expected location",
              name_, config_path.string());
        }
      }
    }

    if (!has_explicit && !req.default_file.empty()) {
      auto default_path = repo_root / req.default_file;

      if (!std::filesystem::exists(default_path)) {
        return fmt::format(
            "Default config file not found for `{}`: {}\n"
            " Run `sniffercommit init` to generate it\n"
            " or create {} manually in the repository root",
            name_, default_path.string(), req.default_file);
      }
    }
  }

  return "";
}

domain::CheckResult ShellCheck::execute(const std::vector<std::string>& files,
                                        domain::ports::IShellExecutor* shell, bool verbose,
                                        bool dry_run) {
  if (!shell->command_exists(command_)) {
    return {.exit_code = 1,
            .output = fmt::format("`{}` not found int PATH. install it or check your configuration",
                                  command_)};
  }

  if (dry_run) {
    return {.exit_code = 0, .output = {}};
  }

  std::string cmd_base = util::shell_escape(command_);
  for (const auto& arg : args_) {
    cmd_base += " ";
    cmd_base += util::shell_escape(arg);
  }

  int overall_exit = 0;
  std::string accumulated_output;
  std::string verbose_log;

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
