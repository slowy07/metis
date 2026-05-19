#include "sniffercommit/config_manager.hpp"

#include <fmt/format.h>

#include <array>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

#include "sniffercommit/cicd_domain.hpp"
#include "sniffercommit/precommit_domain.hpp"
#include "sniffercommit/project_config.hpp"
#include "sniffercommit/tooling_config.hpp"

namespace sniffercommit {

struct PipeDeleter {
  void operator()(FILE* file_ptr) const noexcept {
    if (file_ptr != nullptr) {
      (void)pclose(file_ptr);
    }
  }
};
using PipePtr = std::unique_ptr<FILE, PipeDeleter>;

ConfigManager::InitResult ConfigManager::initialize(const std::filesystem::path& cwd,
                                                    const InitOptions& opts) {
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
  std::string config_content;

  if (opts.enable_clang_tidy) {
    config_content = project::generate_default_with_tidy(
        project_name, tooling::style_name(opts.style), tooling::preset_name(opts.tidy_preset));
  } else {
    config_content = project::generate_default(project_name, tooling::style_name(opts.style));
  }

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

  if (opts.enable_clang_tidy) {
    tooling::ClangTidyConfig tidy_cfg;
    tidy_cfg.preset = opts.tidy_preset;
    tidy_cfg.warnings_as_errors = opts.tidy_severity;
    tidy_cfg.header_filter_level = opts.tidy_header_filter;

    if (auto err = tidy_cfg.validate(); !err.empty()) {
      result.error_message = "Invalid clang-tidy config: " + err;
      return result;
    }

    auto tidy_path = cwd / ".clang-tidy";
    try {
      auto tidy_content = tooling::generate_clang_tidy(tidy_cfg);
      if (!write_file(tidy_path, tidy_content)) {
        result.error_message = "Failed to create " + tidy_path.string();
        return result;
      }
    } catch (const std::exception& error_tidy_path) {
      result.error_message =
          std::string("Failed to generate .clang-tidy: ") + error_tidy_path.what();
      return result;
    }
  }

  if (opts.generate_source) {
    auto src_dir = cwd / "src";

    try {
      if (!std::filesystem::exists(src_dir)) {
        std::filesystem::create_directories(src_dir);
      }
    } catch (const std::exception& error_src_dir) {
      result.error_message =
          std::string("Failed to create src/ directory: ") + error_src_dir.what();
      return result;
    }

    auto main_cpp_path = src_dir / "main.cpp";
    constexpr std::string_view main_cpp_content = R"(#include <iostream>

int main() {
  std::cout << "sniffercommit says wello" << std::endl;
  return 0;
}
)";

    if (!write_file(main_cpp_path, std::string(main_cpp_content))) {
      result.error_message = "Failed to create " + main_cpp_path.string();
      return result;
    }
    result.src_path = main_cpp_path.string();
  }

  // generate CMakeLists.txt
  if (opts.enable_cmake) {
    tooling::CMakeConfig cmake_cfg;
    cmake_cfg.project_name = project_name;
    cmake_cfg.version = "0.2.1";
    cmake_cfg.cpp_standard = opts.cmake_cpp_standard;
    cmake_cfg.target_type = opts.cmake_target_type;
    cmake_cfg.target_name = project_name;
    cmake_cfg.enable_warnings = opts.cmake_enable_warnings;
    cmake_cfg.enable_testing = opts.cmake_enable_testing;
    cmake_cfg.enable_sanitizers = opts.cmake_enable_sanitizers;
    cmake_cfg.enable_clang_format = true;
    cmake_cfg.enable_clang_tidy = opts.enable_clang_tidy;

    cmake_cfg.source_files = {"src/main.cpp"};
    cmake_cfg.include_dirs = {"${CMAKE_CURRENT_SOURCE_DIR}/include"};

    if (auto err = cmake_cfg.validate(); !err.empty()) {
      result.error_message = "Invalid CMake config: " + err;
      return result;
    }

    auto cmake_path = cwd / "CMakeLists.txt";
    try {
      auto cmake_content = tooling::generate_cmake_lists(cmake_cfg);

      if (!write_file(cmake_path, cmake_content)) {
        result.error_message = "Failed to create " + cmake_path.string();
        return result;
      }
    } catch (const std::exception& error_cmake_path) {
      result.error_message =
          std::string("Failed to generate CMakeLists.txt ") + error_cmake_path.what();
      return result;
    }

    result.cmake_config_path = cmake_path.string();
  }

  result.tooling_config_path = clang_path.string();

  result.success = true;
  return result;
}

ConfigManager::InstallResult ConfigManager::install(const std::filesystem::path& repo_root,
                                                    const project::ProjectConfig& cfg) {
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

project::ProjectConfig ConfigManager::load_project(const std::filesystem::path& path) {
  return project::load(path);
}

std::filesystem::path ConfigManager::find_git_root() {
  std::array<char, 4096> buffer{};
  std::string result;
  PipePtr pipe(popen("git rev-parse --show-toplevel 2>/dev/null",
                     "r"));  // NOLINT(bugprone-command-processor)

  if (pipe) {
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
      result += buffer.data();
    }

    if (!result.empty()) {
      if (result.back() == '\n') {
        result.pop_back();
      }
      std::filesystem::path path(result);

      if (std::filesystem::exists(path)) {
        return path;
      }
    }
  }

  auto dir = std::filesystem::current_path();
  while (true) {
    if (std::filesystem::exists(dir / ".git")) {
      return dir;
    }
    auto parent = dir.parent_path();
    if (parent == dir) {
      break;
    }

    dir = parent;
  }

  throw std::runtime_error("Not inside a Git repository for git not in PATH");
}

bool ConfigManager::write_file(const std::filesystem::path& path, const std::string& content) {
  std::ofstream out(path, std::ios::trunc);

  if (!out) {
    return false;
  }

  out << content;
  return out.good();
}

}  // namespace sniffercommit
