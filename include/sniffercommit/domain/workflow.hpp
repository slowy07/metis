#ifndef SNIFFERCOMMIT_DOMAIN_WORKFLOW_HPP
#define SNIFFERCOMMIT_DOMAIN_WORKFLOW_HPP

#include <cstdint>
#include <filesystem>
#include <string>

namespace sniffercommit::domain::config {
struct ProjectConfig;
}

namespace sniffercommit::domain::workflow {

enum class Platform : std::uint8_t { GithubAction, GitLabCI, AzureDevOps, Generic };

struct WorkflowConfig {
  Platform platform = Platform::GithubAction;
  std::string job_name = "Run sniffercommit checks";
  int timeout_minutes = 10;
  bool install_clang_format = false;
  bool install_clang_tidy = false;
  std::string binary_path = "./sniffercommit";
};

// CI/CD generation — pure string generation, no I/O
[[nodiscard]] bool requires_clang_format(const config::ProjectConfig& cfg) noexcept;
[[nodiscard]] bool requires_clang_tidy(const config::ProjectConfig& cfg) noexcept;
[[nodiscard]] std::string generate_workflow(const config::ProjectConfig& cfg,
                                            const WorkflowConfig& wf_cfg);
[[nodiscard]] std::string generate_github_actions(const config::ProjectConfig& cfg,
                                                  const WorkflowConfig& wf_cfg = {});

}  // namespace sniffercommit::domain::workflow

#endif
