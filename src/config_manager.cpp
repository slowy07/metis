#include "sniffercommit/config_manager.hpp"

#include <fmt/format.h>

#include <array>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>

#include "sniffercommit/cicd_domain.hpp"
#include "sniffercommit/precommit_domain.hpp"
#include "sniffercommit/project_config.hpp"
#include "sniffercommit/tooling_config.hpp"

namespace sniffercommit {

ConfigManager::InitResult ConfigManager::initialize(const std::filesystem::path& cwd,
                                                    const InitOptions& opts) const {
  InitResult result;

  std::string project_name = opts.project_name;
  if (project_name.empty()) {
    project_name = cwd.filename().string();
  }

  tooling::ClangFormatConfig clang_cfg;
  clang_cfg.style = opts.style;
  clang_cfg.ident_width = opts.indent_width;
  clang_cfg.column_limit = opts.column_limit;
  clang_cfg.pointer_alignment = opts.pointer_alignment;
  clang_cfg.break_before_braces = opts.brace_style;

  if (auto err = clang_cfg.validate(); !err.empty()) {
    result.error_message = "Invalid clang-format config: " + err;
    return result;
  }

  auto config_path = cwd / ".sniffercommit.toml";
  auto config_content = project::generate_default(project_name, tooling::style_name(opts.style));

  if (!write_file(config_path, config_content)) {
    result.error_message = "Failed to create " + config_path.string();
    return result;
  }

  result.project_config_path = config_path.string();

  auto clang_path = cwd / ".clang-format";
  try {
    auto clang_content = tooling::generate_clang_format(clang_cfg);
    if (!write_file(clang_path, clang_content)) {
      result.error_message = "Failed to create " + clang_path.string();
      return result;
    }
  } catch (const std::exception& error_clang_path) {
    result.error_message =
        std::string("Failed to generate .clang-format: ") + error_clang_path.what();
    return result;
  }

  result.tooling_config_path = clang_path.string();

  result.success = true;
  return result;
}

ConfigManager::InstallResult ConfigManager::install(const std::filesystem::path& repo_root,
                                                    const project::ProjectConfig& cfg) const {
  InstallResult result;

  if (cfg.generate_local_hook) {
    auto hook_content = precommit::generate_hook(cfg);

    if (!precommit::validate_syntax(hook_content)) {
      result.error_message = "Generated hook failed syntax validation";
      return result;
    }

    if (precommit::install(repo_root, hook_content)) {
      result.hook_installed = true;
      result.hook_path = (repo_root / ".git" / "hooks" / "pre-commit").string();
    } else {
      result.error_message = "Failed to install pre-commit hooks";
      return result;
    }
  }

  if (cfg.generate_gha) {
    auto wf_content = cicd::generate_github_actions(cfg, cicd::WorkflowConfig{});

    if (!cicd::write_workflow(repo_root, wf_content)) {
      result.error_message = "Failed to write GitHub Actions workflow";
      return result;
    }

    result.workflow_installed = true;
    result.workflow_path = (repo_root / ".github" / "workflows" / "sniffercommit.yml").string();
  }

  return result;
}

project::ProjectConfig ConfigManager::load_project(const std::filesystem::path& path) const {
  return project::load(path);
}

std::filesystem::path ConfigManager::find_git_root() const {
  std::array<char, 1024> buffer;
  std::string result;
  std::unique_ptr<FILE, decltype(&pclose)> pipe(popen("git rev-parse --show-toplevel 2>/dev/null", "r"), pclose);

  if (pipe) {
    while (fgets(buffer.data(), buffer.size(), pipe.get())) {
      result += buffer.data();
    }

    if (!result.empty()) {
      if (result.back() == '\n') result.pop_back();
      std::filesystem::path path(result);
      
      if (std::filesystem::exists(path)) {
        return path;
      }
    }
  }

  auto dir = std::filesystem::current_path();
  while (true) {
    if (std::filesystem::exists(dir / ".git")) return dir;
    auto parent = dir.parent_path();
    if (parent == dir) {
      break;
    }

    dir = parent;
  }

  throw std::runtime_error("Not inside a Git repository for git not in PATH");
}

bool ConfigManager::write_file(const std::filesystem::path& path, const std::string& content) const {
  std::ofstream out(path);
  if (!out) {
    return false;
  }

  out << content;
  return out.good();
}

}  // namespace sniffercommit
