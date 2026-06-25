#include "sniffercommit/domain/config.hpp"

#include <fmt/format.h>

#include <algorithm>
#include <filesystem>
#include <string>
#include <string_view>

namespace sniffercommit::domain::config {

std::string Check::validate() const noexcept {
  if (name.empty()) {
    return "Check name cannot be empty";
  }
  if (command.empty()) {
    return fmt::format("Check `{}` missing require `command`", name);
  }
  return "";
}

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

bool ProjectConfig::has_command(std::string_view cmd) const noexcept {
  return std::ranges::any_of(checks, [cmd](const auto& check) { return check.command == cmd; });
}

bool ProjectConfig::has_matching_checks(const std::string& file) const noexcept {
  for (const auto& check : checks) {
    for (const auto& pattern : check.patterns) {
      if (pattern == "*") {
        return true;
      }
      if (pattern.starts_with("*.") && file.ends_with(pattern.substr(1))) {
        return true;
      }
      if (pattern.ends_with("/**") &&
          file.starts_with(pattern.substr(0, pattern.size() - 3) + "/")) {
        return true;
      }
      if (pattern.starts_with("**/")) {
        std::string suffix = pattern.substr(3);
        if (file.ends_with(suffix)) {
          return true;
        }
      }
      if (file.starts_with(pattern) || file == pattern) {
        return true;
      }
    }
  }
  return false;
}

static std::string make_config_file_arg(const std::filesystem::path& repo_root,
                                        const std::string& filename) {
  auto abs_path = std::filesystem::absolute(repo_root / filename);
  return "--config-file=" + abs_path.string();
}

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
