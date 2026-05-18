#ifndef SNIFFERCOMMIT_CICD_DOMAIN_HPP
#define SNIFFERCOMMIT_CICD_DOMAIN_HPP

#include <cstdint>
#include <filesystem>

namespace sniffercommit::project {
struct ProjectConfig;
}

namespace sniffercommit::cicd {

enum class Platform : std::uint8_t { GithubAction, GitLabCI, AzureDevOps, Generic };

struct WorkflowConfig {
  Platform platform = Platform::GithubAction;
  std::string job_name = "Run sniffercommit checks";
  int timeout_minutes = 10;
  bool install_clang_format = false;
  bool install_clang_tidy = false;
  std::string binary_path = "./sniffercommit";
};

// INFO: generate workflow from project config
[[nodiscard]] std::string generate_workflow(const project::ProjectConfig& cfg,
                                            const WorkflowConfig& wf_cfg);
// INFO: action specific config (Github)
[[nodiscard]] std::string generate_github_actions(const project::ProjectConfig& cfg,
                                                  const WorkflowConfig& wf_cfg = {});
// INFO: workflow to filesystem
[[nodiscard]] bool write_workflow(const std::filesystem::path& repo_root,
                                  const std::string& content,
                                  Platform platform = Platform::GithubAction);
// INFO: auto-detect project needs clang-format installation
[[nodiscard]] bool requires_clang_format(const project::ProjectConfig& cfg) noexcept;

// INFO: auto detect project needs clang-tidy installation
[[nodiscard]] bool requires_clang_tidy(const project::ProjectConfig& cfg) noexcept;

}  // namespace sniffercommit::cicd

#endif  // !SNIFFERCOMMIT_CICD_DOMAIN_HPP
