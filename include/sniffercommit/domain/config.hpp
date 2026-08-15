#ifndef SNIFFERCOMMIT_DOMAIN_CONFIG_HPP
#define SNIFFERCOMMIT_DOMAIN_CONFIG_HPP

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "sniffercommit/domain/ports/shell_executor.hpp"

namespace sniffercommit::domain::config {

// A single check definition from the config file.
// Each check specifies a command, its arguments, and file patterns to match.
struct Check {
  std::string name;
  std::string description;
  bool enabled = true;
  std::string command;
  std::vector<std::string> args;
  std::vector<std::string> patterns;
  int timeout = 0;
  std::string severity = "error";

  [[nodiscard]] std::string validate() const noexcept;
};

// Top-level project configuration loaded from .sniffercommit.toml.
// Contains project metadata, check definitions, and output preferences.
struct ProjectConfig {
  std::string project_name = "unnamed";
  std::vector<Check> checks;
  std::vector<std::string> exclude_paths;
  bool generate_local_hook = true;
  bool generate_gha = false;
  bool generate_gitlab_ci = false;
  bool parallel = true;

  [[nodiscard]] std::string validate() const noexcept;
  [[nodiscard]] bool is_valid() const noexcept { return validate().empty(); }
  [[nodiscard]] bool has_command(std::string_view cmd) const noexcept;
};

// Default config string generators (pure string generation, no I/O).
// These produce TOML content for .sniffercommit.toml with different check sets.
[[nodiscard]] std::string generate_default_config(const std::string& project_name,
                                                  const std::string& fallback_style = "Google",
                                                  const std::filesystem::path& repo_root = ".");
[[nodiscard]] std::string generate_default_config_with_tidy(
    const std::string& project_name, const std::string& fallback_style = "Google",
    const std::string& tidy_preset = "standard", const std::filesystem::path& repo_root = ".");
[[nodiscard]] std::string generate_default_config_with_cmake(
    const std::string& project_name, const std::string& fallback_style = "Google");

}  // namespace sniffercommit::domain::config

#endif
