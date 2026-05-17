#include "sniffercommit/cicd_domain.hpp"

#include <fmt/format.h>

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <system_error>

#include "sniffercommit/project_config.hpp"

namespace sniffercommit::cicd {
bool requires_clang_format(const project::ProjectConfig& cfg) noexcept {
  return cfg.has_command("clang-format");
}

std::string generate_github_actions(const project::ProjectConfig& cfg,
                                    const WorkflowConfig& wf_cfg) {
  std::string yml;

  yml += R"(name: sniffercommit

on:
  push:
    branches:
      - "**"

  pull_request:
    branches:
      - "**"

concurrency:
  group: sniffercommit-${{ github.ref }}
  cancel-in-progress: true

permissions:
  contents: read

jobs:
  checks:
)";

  yml += fmt::format(R"(    name: {}
)",
                     wf_cfg.job_name);

  yml += "    runs-on: ubuntu-latest\n";

  yml += fmt::format(R"(    timeout-minutes: {}

)",
                     wf_cfg.timeout_minutes);

  yml += R"(    steps:
      - name: Checkout repository
        uses: actions/checkout@v4
        with:
          fetch-depth: 0

)";

  bool need_clang = wf_cfg.install_clang_format || requires_clang_format(cfg);

  if (need_clang) {
    yml += R"(      - name: Install clang-format
        run: |
          sudo apt-get update
          sudo apt-get install -y clang-format

)";
  }

  yml += R"(      - name: Make sniffercommit executable
        run: chmod +x ./sniffercommit

      - name: Run sniffercommit
        shell: bash
        run: |
          set -euo pipefail
          ./sniffercommit run --all-files --verbose
)";

  return yml;
}

std::string generate_workflow(const project::ProjectConfig& cfg, const WorkflowConfig& wf_cfg) {
  switch (wf_cfg.platform) {
    case Platform::GithubAction:
      return generate_github_actions(cfg, wf_cfg);

      // TODO: implement Gitlab, Azure
    case Platform::GitLabCI:
      throw std::runtime_error("GitLab CI not yet implemented");
    case Platform::AzureDevOps:
      throw std::runtime_error("Azure DevOps not yet implemented");
    case Platform::Generic:
      return generate_github_actions(cfg, wf_cfg);
  }

  return generate_github_actions(cfg, wf_cfg);
}

bool write_workflow(const std::filesystem::path& repo_root, const std::string& content,
                    Platform platform) {
  auto gh_dir = repo_root / ".github" / "workflows";

  std::filesystem::create_directories(gh_dir);

  std::string filename;
  switch (platform) {
    case Platform::GithubAction:
      filename = "sniffercommit.yml";
      break;
    case Platform::GitLabCI:
      filename = ".gitlab-ci.yml";
      break;
    default:
      filename = "sniffercommit.yml";
      break;
  }

  auto yml_path = gh_dir / filename;
  std::ofstream out(yml_path, std::ios::trunc);
  if (!out) return false;
  out << content;
  return out.good();
}

}  // namespace sniffercommit::cicd
