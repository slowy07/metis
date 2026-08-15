#include "sniffercommit/domain/config.hpp"

#include <fmt/format.h>

#include <algorithm>
#include <filesystem>
#include <string>
#include <string_view>

#include "sniffercommit/util.hpp"

namespace sniffercommit::domain::config {

// Validates a single check definition.
// Returns empty string if valid, error message if not.
std::string Check::validate() const noexcept {
  if (name.empty()) {
    return "Check name cannot be empty";
  }
  if (command.empty()) {
    return fmt::format("Check `{}` missing require `command`", name);
  }
  if (timeout < 0) {
    return fmt::format("Check `{}` timeout cannot be negative", name);
  }
  if (severity != "error" && severity != "warning" && severity != "info") {
    return fmt::format("Check `{}` severity must be 'error', 'warning', or 'info'", name);
  }
  return "";
}

// Builds the escaped shell command: command + args + files.
// Every check type (clang-format, grep, ...) uses the same shape,
// so execution is generic instead of per-tool.
std::string Check::command_line(const std::vector<std::string>& files) const {
  std::string cmd = util::shell_escape(command);
  for (const auto& arg : args) {
    cmd += " ";
    cmd += util::shell_escape(arg);
  }
  for (const auto& file : files) {
    cmd += " ";
    cmd += util::shell_escape(file);
  }
  return cmd;
}

// Runs the check against all given files in one shell invocation.
// has no timeout support yet. Add when a check needs one.
domain::ports::CapturedResult Check::execute(domain::ports::IShellExecutor& shell,
                                             const std::vector<std::string>& files) const {
  return shell.exec_captured(command_line(files));
}

// Validates the entire project config.
// Checks: non-empty project name, at least one check, valid checks,
// and no duplicate check names.
std::string ProjectConfig::validate() const noexcept {
  if (project_name.empty()) {
    return "Project name cannot be empty";
  }
  if (checks.empty()) {
    return "Config must contain at least one [[checks]] entry";
  }
  for (const auto& check : checks) {
    if (auto err = check.validate(); !err.empty()) {
      return err;
    }
  }
  for (size_t i = 0; i < checks.size(); ++i) {
    for (size_t j = i + 1; j < checks.size(); ++j) {
      if (checks.at(i).name == checks.at(j).name) {
        return fmt::format("Duplicate check name: `{}`", checks.at(i).name);
      }
    }
  }
  return "";
}

// Checks if any check in the config uses the given command.
// Used to determine if clang-format/clang-tidy checks are configured.
bool ProjectConfig::has_command(std::string_view cmd) const noexcept {
  return std::ranges::any_of(checks, [cmd](const auto& check) { return check.command == cmd; });
}

// Builds a --config-file=... argument for a tool, using an absolute path.
// Used by generate_default_config_with_tidy to embed the .clang-tidy path.
static std::string make_config_file_arg(const std::filesystem::path& repo_root,
                                        const std::string& filename) {
  auto abs_path = std::filesystem::absolute(repo_root / filename);
  return "--config-file=" + abs_path.string();
}

// Generates a basic .sniffercommit.toml with clang-format and trailing-whitespace checks.
// This is the default config when no special options are enabled.
std::string generate_default_config(const std::string& project_name,
                                    const std::string& fallback_style,
                                    const std::filesystem::path& repo_root) {
  (void)repo_root;
  return fmt::format(
      R"([project]
name = "{}"

[[checks]]
name = "clang-format"
command = "clang-format"
args = [
  "-i",
  "--fallback-style={}",
  "-style=file"
]
patterns = ["*.cpp", "*.hpp", "*.h", "*.cc"]

[[checks]]
name = "trailing-whitespace"
command = "grep"
args = ["-E", "--text", "[[:space:]]+$"]
patterns = ["*"]

[exclude]
paths = ["build/", "third_party/", ".git/"]

[output]
local_hook = true
github_actions = false

[execution]
parallel = true
)",
      project_name, fallback_style);
}

// Like generate_default_config(), but adds a clang-tidy check.
// The tidy config path is embedded as an absolute path in the check args.
std::string generate_default_config_with_tidy(const std::string& project_name,
                                              const std::string& fallback_style,
                                              const std::string& tidy_preset,
                                              const std::filesystem::path& repo_root) {
  (void)tidy_preset;
  std::string tidy_config_arg = make_config_file_arg(repo_root, ".clang-tidy");
  return fmt::format(
      R"([project]
name = "{}"

[[checks]]
name = "clang-format"
command = "clang-format"
args = ["-i", "--fallback-style={}", "-style=file"]
patterns = ["*.cpp", "*.hpp", "*.h", "*.cc"]

[[checks]]
name = "clang-tidy"
command = "clang-tidy"
args = [
  "{}",
  "--quiet"
]

patterns = ["*.cpp", "*.hpp", "*.h", "*.cc"]

[[checks]]
name = "trailing-whitespace"
command = "grep"
args = ["-E", "--text", "[[:space:]]+$"]
patterns = ["*"]

[exclude]
paths = ["build/", "third_party/", ".git/"]

[output]
local_hook = true
github_actions = false

[execution]
parallel = true
)",
      project_name, fallback_style, tidy_config_arg);
}

// Generates config with clang-format + cmake-format checks.
// lazy: never called — dead code. The cmake format check was added
// speculatively but init_use_case.cpp doesn't use this variant.
std::string generate_default_config_with_cmake(const std::string& project_name,
                                               const std::string& fallback_style) {
  return fmt::format(
      R"([project]
name = "{}"

[[checks]]
name = "clang-format"
command = "clang-format"
args = ["-i", "--fallback-style={}", "-style=file"]
patterns = ["*.cpp", "*.hpp", "*.h", "*.cc"]

[[checks]]
name = "cmake-format"
command = "cmake-format"
args = ["-i"]
patterns = ["CMakeLists.txt", "*.cmake"]

[[checks]]
name = "trailing-whitespace"
command = "grep"
args = ["-E", "--text", "[[:space:]]+$"]
patterns = ["*"]

[exclude]
paths = ["build/", "third_party/", ".git/"]

[output]
local_hook = true
github_actions = false

[execution]
parallel = true
)",
      project_name, fallback_style);
}

}  // namespace sniffercommit::domain::config
