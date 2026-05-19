#ifndef SNIFFERCOMMIT_CONFIG_MANAGER_HPP
#define SNIFFERCOMMIT_CONFIG_MANAGER_HPP

#include <filesystem>
#include <string>

#include "sniffercommit/project_config.hpp"
#include "sniffercommit/tooling_config.hpp"

namespace sniffercommit {

class ConfigManager {
 public:
  struct InitOptions {
    std::string project_name;
    tooling::FormatterStyle style = tooling::FormatterStyle::Google;
    int indent_width = 2;
    int column_limit = 100;
    std::string pointer_alignment = "Left";
    std::string brace_style = "Attach";
    bool enable_clang_tidy = false;

    tooling::TidyPreset tidy_preset = tooling::TidyPreset::Standard;
    tooling::TidySeverity tidy_severity = tooling::TidySeverity::Error;
    int tidy_header_filter = 1;

    // cmake scaffolding
    bool enable_cmake = false;
    tooling::CppStandard cmake_cpp_standard = tooling::CppStandard::Cpp20;
    tooling::TargetType cmake_target_type = tooling::TargetType::Executable;
    bool cmake_enable_warnings = true;
    bool cmake_enable_testing = false;
    bool cmake_enable_sanitizers = false;
  };

  struct InitResult {
    bool success = false;
    std::string project_config_path;
    std::string tooling_config_path;
    std::string cmake_config_path;
    std::string error_message;
  };

  struct InstallResult {
    bool hook_installed = false;
    bool workflow_installed = false;
    std::string hook_path;
    std::string workflow_path;
    std::string error_message;
  };

  // INFO: initialize  project configs (`init` command logic)
  [[nodiscard]] static InitResult initialize(const std::filesystem::path& cwd,
                                             const InitOptions& opts);
  // INFO: install hooks and CI (`install` command logic)
  [[nodiscard]] static InstallResult install(const std::filesystem::path& repo_root,
                                             const project::ProjectConfig& cfg);
  // INFO: load project config with validation
  [[nodiscard]] static project::ProjectConfig load_project(const std::filesystem::path& path);
  // INFO: finding git repository root
  [[nodiscard]] static std::filesystem::path find_git_root();

 private:
  [[nodiscard]] static bool write_file(const std::filesystem::path& path,
                                       const std::string& content);
};
}  // namespace sniffercommit

#endif  // !SNIFFERCOMMIT_CONFIG_MANAGER_HPP
