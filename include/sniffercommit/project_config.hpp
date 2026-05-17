#ifndef SNIFFERCOMMIT_PROJECT_CONFIG_HPP
#define SNIFFERCOMMIT_PROJECT_CONFIG_HPP

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace sniffercommit::project {
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
  bool parallel = true;

  [[nodiscard]] std::string validate() const noexcept;
  bool is_valid() const noexcept {
    return validate().empty();
  }

  [[nodiscard]] bool has_command(std::string_view cmd) const noexcept;
  [[nodiscard]] bool has_matching_checks(const std::string& file) const noexcept;
};

// INFO: load and save toml files
// generate defailt configs string (for `init` command)
[[nodiscard]] ProjectConfig load(const std::filesystem::path& path);
bool save(const std::filesystem::path& path, const ProjectConfig& cfg);
[[nodiscard]] std::string generate_default(const std::string& project_name,
                                           const std::string& fallback_style = "Google");

}  // namespace sniffercommit::project

#endif  // !SNIFFERCOMMIT_PROJECT_CONFIG_HPP
