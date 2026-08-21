#include "metis/domain/workflow.hpp"

#include <fmt/format.h>

#include <string>

#include "metis/domain/config.hpp"

namespace metis::domain::workflow {

// Checks if the project config includes clang-format checks.
bool requires_clang_format(const config::ProjectConfig& cfg) noexcept {
  return cfg.has_command("clang-format");
}

bool requires_clang_tidy(const config::ProjectConfig& cfg) noexcept {
  return cfg.has_command("clang-tidy");
}

namespace {

// Generates the GitHub Actions setup step that installs clang-format/clang-tidy.
// Only added if the config uses these tools.
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

// Generates the GitLab CI before_script that installs clang-format/clang-tidy.
// lazy: same logic as generate_gha_setup_step but different YAML format.
// Could be unified, but the YAML templates are different enough that
// keeping them separate is clearer.
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

// Generates a GitHub Actions workflow YAML.
// Triggers on push and PR to all branches, with concurrency control.
// The workflow checks out the repo, optionally installs LLVM tools,
// then runs metis on all files.
std::string generate_github_actions(const config::ProjectConfig& cfg,
                                    const WorkflowConfig& wf_cfg) {
  bool need_clang_format = wf_cfg.install_clang_format || requires_clang_format(cfg);
  bool need_clang_tidy = wf_cfg.install_clang_tidy || requires_clang_tidy(cfg);

  return fmt::format(
      R"yaml(name: metis

on:
  push:
    branches:
      - "**"
  pull_request:
    branches:
      - "**"

concurrency:
  group: metis-${{ github.ref }}
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
      - name: Make metis executable
        run: chmod +x {binary_path}

      - name: Run metis
        shell: bash
        run: |
          set -euo pipefail
          {binary_path} run --all-files --verbose
)yaml",
      fmt::arg("job_name", wf_cfg.job_name), fmt::arg("timeout", wf_cfg.timeout_minutes),
      fmt::arg("binary_path", wf_cfg.binary_path),
      fmt::arg("setup_step", generate_gha_setup_step(need_clang_format, need_clang_tidy)));
}

// Generates a GitLab CI pipeline YAML.
// Single-stage pipeline that runs metis on all files.
// Uses ubuntu:latest image, installs LLVM tools if needed.
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

// Dispatches to the appropriate platform-specific generator.
// lazy: switch with __builtin_unreachable() for GCC/Clang to suppress
// "not all control paths return a value" warning. MSVC path returns
// a default as fallback.
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

}  // namespace metis::domain::workflow
