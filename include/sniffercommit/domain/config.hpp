#ifndef SNIFFERCOMMIT_DOMAIN_CONFIG_HPP
#define SNIFFERCOMMIT_DOMAIN_CONFIG_HPP

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace sniffercommit::domain::config {

struct Check {
  std::string name;
  std::string command;
  std::vector<std::string> args;
  std::vector<std::string> patterns;

  [[nodiscard]] std::string validate() const noexcept;
};

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

// default config string generators (pure string generation, no I/O)
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
