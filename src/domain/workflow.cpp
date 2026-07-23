#include "sniffercommit/domain/workflow.hpp"

#include <fmt/format.h>

#include <memory>
#include <stdexcept>
#include <string>

#include "sniffercommit/domain/config.hpp"

namespace sniffercommit::domain::workflow {

bool requires_clang_format(const config::ProjectConfig& cfg) noexcept {
  return cfg.has_command("clang-format");
}

bool requires_clang_tidy(const config::ProjectConfig& cfg) noexcept {
  return cfg.has_command("clang-tidy");
}

class GithubActionsGenerator : public IWorkflowGenerator {
 public:
  [[nodiscard]] std::string generate(const config::ProjectConfig& cfg,
                                     const WorkflowConfig& wf_cfg) const override {
    bool need_clang_format = wf_cfg.install_clang_format || requires_clang_format(cfg);
    bool need_clang_tidy = wf_cfg.install_clang_tidy || requires_clang_tidy(cfg);

    std::string setup_step = generate_setup_step(need_clang_format, need_clang_tidy);

    return fmt::format(
        R"yaml(name: sniffercommit

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
    name: {job_name}
    runs-on: ubuntu-latest
    timeout-minutes: {timeout}
    steps:
      - name: Checkout repository
        uses: actions/checkout@v4
        with:
          fetch-depth: 0
{setup_step}
      - name: Make sniffercommit executable
        run: chmod +x {binary_path}

      - name: Run sniffercommit
        shell: bash
        run: |
          set -euo pipefail
          {binary_path} run --all-files --verbose
)yaml",
        fmt::arg("job_name", wf_cfg.job_name), fmt::arg("timeout", wf_cfg.timeout_minutes),
        fmt::arg("binary_path", wf_cfg.binary_path), fmt::arg("setup_step", setup_step));
  }

 private:
  [[nodiscard]] std::string generate_setup_step(bool need_clang_format,
                                                bool need_clang_tidy) const {
    if (!need_clang_format && !need_clang_tidy) {
      return "";
    }

    std::vector<std::string> packages;
    if (need_clang_format) {
      packages.emplace_back("clang-format");
    }
    if (need_clang_tidy) {
      packages.emplace_back("clang-tidy");
    }

    std::string package_list;
    for (size_t i = 0; i < packages.size(); ++i) {
      if (i > 0) {
        package_list += " ";
      }
      package_list += packages[i];
    }

    return fmt::format(
        R"yaml(      - name: Install LLVM tooling
        run: |
          sudo apt-get update
          sudo apt-get install -y {packages}
)yaml",
        fmt::arg("packages", package_list));
  }
};

class GitLabCIGenerator : public IWorkflowGenerator {
 public:
  [[nodiscard]] std::string generate(const config::ProjectConfig& cfg,
                                     const WorkflowConfig& wf_cfg) const override {
    bool need_clang_format = wf_cfg.install_clang_format || requires_clang_format(cfg);
    bool need_clang_tidy = wf_cfg.install_clang_tidy || requires_clang_tidy(cfg);

    return fmt::format(
        R"yaml(stages:
  - check

{job_name}:
  stage: check
  image: ubuntu:latest
  timeout: {timeout}m
  script:
    - apt-get update
    - apt-get install -y {packages}
    - chmod +x {binary_path}
    - {binary_path} run --all-files --verbose
  rules:
    - if: $CI_PIPELINE_SOURCE == "push"
    - if: $CI_PIPELINE_SOURCE == "merge_request_event"
)yaml",
        fmt::arg("job_name", wf_cfg.job_name), fmt::arg("timeout", wf_cfg.timeout_minutes),
        fmt::arg("binary_path", wf_cfg.binary_path),
        fmt::arg("packages", format_packages(need_clang_format, need_clang_tidy)));
  }

 private:
  [[nodiscard]] static std::string format_packages(bool need_clang_format, bool need_clang_tidy) {
    std::string pkgs = "build-essential";
    if (need_clang_format) pkgs += " clang-format";
    if (need_clang_tidy) pkgs += " clang-tidy";
    return pkgs;
  }
};

std::unique_ptr<IWorkflowGenerator> create_generator(Platform platform) {
  switch (platform) {
    default:
    case Platform::GithubAction:
      return std::make_unique<GithubActionsGenerator>();
    case Platform::GitLabCI:
      return std::make_unique<GitLabCIGenerator>();
    case Platform::AzureDevOps:
      throw std::runtime_error("Azure DevOps generator is not yet implemented");
    case Platform::Generic:
      throw std::runtime_error(
          "Generic platform requires explicit CI/CD target (e.g GithubAction)");
      break;
  }

  throw std::runtime_error("Unknown workflow platform detected");
}

std::string generate_github_actions(const config::ProjectConfig& cfg,
                                    const WorkflowConfig& wf_cfg) {
  return GithubActionsGenerator().generate(cfg, wf_cfg);
}

std::string generate_workflow(const config::ProjectConfig& cfg, const WorkflowConfig& wf_cfg) {
  switch (wf_cfg.platform) {
    case Platform::GithubAction:
      return generate_github_actions(cfg, wf_cfg);
    case Platform::GitLabCI:
      return GitLabCIGenerator().generate(cfg, wf_cfg);
    case Platform::AzureDevOps:
      throw std::runtime_error("Azure DevOps not yet implemented");
    case Platform::Generic:
      return generate_github_actions(cfg, wf_cfg);
  }
#if defined(__GNUC__) || defined(__clang__)
  __builtin_unreachable();
#else
  return generate_github_actions(cfg, wf_cfg);
#endif
}

}  // namespace sniffercommit::domain::workflow
