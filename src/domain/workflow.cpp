#include "sniffercommit/domain/workflow.hpp"

#include <fmt/format.h>

#include <string>

#include "sniffercommit/domain/config.hpp"

namespace sniffercommit::domain::workflow {

bool requires_clang_format(const config::ProjectConfig& cfg) noexcept {
  return cfg.has_command("clang-format");
}

bool requires_clang_tidy(const config::ProjectConfig& cfg) noexcept {
  return cfg.has_command("clang-tidy");
}

namespace {

std::string generate_gha_setup_step(bool need_clang_format, bool need_clang_tidy) {
  if (!need_clang_format && !need_clang_tidy) {
    return "";
  }

  std::string packages;
  if (need_clang_format) packages += "clang-format";
  if (need_clang_tidy) packages += (packages.empty() ? "" : " ") + std::string("clang-tidy");

  return fmt::format(
      R"yaml(      - name: Install LLVM tooling
        run: |
          sudo apt-get update
          sudo apt-get install -y {packages}
)yaml",
      fmt::arg("packages", packages));
}

std::string generate_gitlab_before_script(bool need_clang_format, bool need_clang_tidy) {
  if (!need_clang_format && !need_clang_tidy) {
    return "";
  }

  std::string pkgs;
  if (need_clang_format) pkgs += " clang-format";
  if (need_clang_tidy) pkgs += " clang-tidy";

  return fmt::format(
      R"yaml(  before_script:
    - apt-get update -qq
    - apt-get install -y -qq{packages}
)yaml",
      fmt::arg("packages", pkgs));
}

}  // namespace

std::string generate_github_actions(const config::ProjectConfig& cfg,
                                    const WorkflowConfig& wf_cfg) {
  bool need_clang_format = wf_cfg.install_clang_format || requires_clang_format(cfg);
  bool need_clang_tidy = wf_cfg.install_clang_tidy || requires_clang_tidy(cfg);

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
      fmt::arg("binary_path", wf_cfg.binary_path),
      fmt::arg("setup_step", generate_gha_setup_step(need_clang_format, need_clang_tidy)));
}

std::string generate_gitlab_ci(const config::ProjectConfig& cfg, const WorkflowConfig& wf_cfg) {
  bool need_clang_format = wf_cfg.install_clang_format || requires_clang_format(cfg);
  bool need_clang_tidy = wf_cfg.install_clang_tidy || requires_clang_tidy(cfg);

  return fmt::format(
      R"yaml(stages:
  - check

{job_name}:
  stage: check
  image: ubuntu:latest
  timeout: {timeout}m
{before_script}  script:
    - set -e
    - chmod +x {binary_path}
    - {binary_path} run --all-files --verbose
  rules:
    - if: $CI_PIPELINE_SOURCE == "push"
    - if: $CI_PIPELINE_SOURCE == "merge_request_event"
)yaml",
      fmt::arg("job_name", wf_cfg.job_name), fmt::arg("timeout", wf_cfg.timeout_minutes),
      fmt::arg("binary_path", wf_cfg.binary_path),
      fmt::arg("before_script", generate_gitlab_before_script(need_clang_format, need_clang_tidy)));
}

std::string generate_workflow(const config::ProjectConfig& cfg, const WorkflowConfig& wf_cfg) {
  switch (wf_cfg.platform) {
    case Platform::GithubAction:
      return generate_github_actions(cfg, wf_cfg);
    case Platform::GitLabCI:
      return generate_gitlab_ci(cfg, wf_cfg);
  }
#if defined(__GNUC__) || defined(__clang__)
  __builtin_unreachable();
#else
  return generate_github_actions(cfg, wf_cfg);
#endif
}

}  // namespace sniffercommit::domain::workflow
