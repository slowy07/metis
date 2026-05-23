#include "sniffercommit/cicd_domain.hpp"

#include <fmt/format.h>

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

#include "sniffercommit/project_config.hpp"

namespace sniffercommit::cicd {
bool requires_clang_format(const project::ProjectConfig& cfg) noexcept {
  return cfg.has_command("clang-format");
}

bool requires_clang_tidy(const project::ProjectConfig& cfg) noexcept {
  return cfg.has_command("clang-tidy");
}

std::string generate_github_actions(const project::ProjectConfig& cfg,
                                    const WorkflowConfig& wf_cfg) {
  std::string yml;

  yml += "name: sniffercommit\n\n";

  yml += "on:\n";
  yml += "  push:\n";
  yml += "    branches:\n";
  yml += "      - \"**\"\n\n";

  yml += "  pull_request:\n";
  yml += "    branches:\n";
  yml += "      - \"**\"\n\n";

  yml += "concurrency:\n";
  yml += "  group: sniffercommit-${{ github.ref }}\n";
  yml += "  cancel-in-progress: true\n\n";

  yml += "permissions:\n";
  yml += "  contents: read\n\n";

  yml += "jobs:\n";
  yml += "  checks:\n";
  yml += fmt::format("    name: {}\n", wf_cfg.job_name);
  yml += "    runs-on: ubuntu-latest\n";
  yml += fmt::format("    timeout-minutes: {}\n\n", wf_cfg.timeout_minutes);

  yml += "    steps:\n";
  yml += "      - name: Checkout repository\n";
  yml += "        uses: actions/checkout@v4\n";
  yml += "        with:\n";
  yml += "          fetch-depth: 0\n\n";

  bool need_clang_format = wf_cfg.install_clang_format || requires_clang_format(cfg);
  bool need_clang_tidy = wf_cfg.install_clang_tidy || requires_clang_tidy(cfg);

  if (need_clang_format || need_clang_tidy) {
    yml += "      - name: Install clang-format\n";
    yml += "        run: |\n";

    if (need_clang_format) {
      yml += "           sudo apt-get update\n";
      yml += "           sudo apt-get install -y clang-format\n";
    }

    if (need_clang_tidy) {
      yml += "           sudo apt-get install -y clang-tidy\n";
    }
    yml += "\n";
  }

  yml += "      - name: Make sniffercommit executable\n";
  yml += "        run: chmod +x ./sniffercommit\n\n";

  yml += "      - name: Run sniffercommit\n";
  yml += "        shell: bash\n";
  yml += "        run: |\n";
  yml += "          set -euo pipefail\n";
  yml += "          ./sniffercommit run --all-files --verbose\n";

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
  std::filesystem::path dir;
  std::string filename;

  switch (platform) {
    case Platform::GithubAction:
      dir = repo_root / ".github" / "workflows";
      filename = "sniffercommit.yml";
      break;
    case Platform::GitLabCI:
      dir = repo_root;
      filename = ".gitlab-ci.yml";
      break;
    case Platform::AzureDevOps:
      dir = repo_root / ".azure-pipeline";
      filename = "sniffercommit.yml";
      break;
    case Platform::Generic:
      dir = repo_root / ".github" / "workflows";
      filename = "sniffercommit.yml";
      break;
    default:
      dir = repo_root;
      filename = "sniffercommit.yml";
      break;
  }

  std::filesystem::create_directories(dir);

  auto yml_path = dir / filename;
  std::ofstream out(yml_path, std::ios::trunc);
  if (!out) {
    return false;
  }

  out << content;
  return out.good();
}

}  // namespace sniffercommit::cicd
