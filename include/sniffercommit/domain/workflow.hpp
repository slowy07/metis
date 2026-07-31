#ifndef SNIFFERCOMMIT_DOMAIN_WORKFLOW_HPP
#define SNIFFERCOMMIT_DOMAIN_WORKFLOW_HPP

#include <cstdint>
#include <string>

namespace sniffercommit::domain::config {
struct ProjectConfig;
}

namespace sniffercommit::domain::workflow {

// Supported CI platforms for workflow generation.
enum class Platform : std::uint8_t { GithubAction, GitLabCI };

// Configuration for generated CI workflow files.
struct WorkflowConfig {
  Platform platform = Platform::GithubAction;
  std::string job_name = "Run sniffercommit checks";
  int timeout_minutes = 10;
  bool install_clang_format = false;
  bool install_clang_tidy = false;
  std::string binary_path = "./sniffercommit";
};

// Checks if the config requires clang-format/clang-tidy installation in CI.
[[nodiscard]] bool requires_clang_format(const config::ProjectConfig& cfg) noexcept;
[[nodiscard]] bool requires_clang_tidy(const config::ProjectConfig& cfg) noexcept;

// Generates workflow content for the specified platform.
[[nodiscard]] std::string generate_workflow(const config::ProjectConfig& cfg,
                                            const WorkflowConfig& wf_cfg);

// Convenience wrappers that set platform and call generate_workflow.
[[nodiscard]] std::string generate_github_actions(const config::ProjectConfig& cfg,
                                                  const WorkflowConfig& wf_cfg = {});
[[nodiscard]] std::string generate_gitlab_ci(const config::ProjectConfig& cfg,
                                             const WorkflowConfig& wf_cfg = {});

}  // namespace sniffercommit::domain::workflow

#endif
