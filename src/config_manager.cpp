#include "sniffercommit/config_manager.hpp"

#include <fmt/format.h>

#include <array>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

#include "sniffercommit/cicd_domain.hpp"
#include "sniffercommit/precommit_domain.hpp"
#include "sniffercommit/project_config.hpp"
#include "sniffercommit/tooling_config.hpp"
#include "sniffercommit/util.hpp"

namespace sniffercommit {

namespace {
[[nodiscard]] tooling::ClangFormatConfig make_clang_format(const ConfigManager::InitOptions& opts) {
  tooling::ClangFormatConfig clang_cfg;
  clang_cfg.style = opts.style;
  clang_cfg.indent_width = opts.indent_width;
  clang_cfg.column_limit = opts.column_limit;
  clang_cfg.pointer_alignment = opts.pointer_alignment;
  clang_cfg.break_before_braces = opts.brace_style;
  return clang_cfg;
}

[[nodiscard]] ConfigManager::InitResult write_project_config(
    const std::filesystem::path& cwd, const std::string& project_name,
    const ConfigManager::InitOptions& opts) {
  ConfigManager::InitResult result;
  auto config_path = cwd / ".sniffercommit.toml";
  std::string config_content;

  if (opts.enable_clang_tidy) {
    config_content = project::generate_default_with_tidy(
        project_name, tooling::style_name(opts.style), tooling::preset_name(opts.tidy_preset));
  } else {
    config_content = project::generate_default(project_name, tooling::style_name(opts.style));
  }

  if (!ConfigManager::write_file(config_path, config_content)) {
    result.error_message = "Failed to create " + config_path.string();
    return result;
  }

  result.project_config_path = config_path.string();
  result.success = true;
  return result;
}

[[nodiscard]] ConfigManager::InitResult write_clang_format(
    const std::filesystem::path& cwd, const tooling::ClangFormatConfig& clang_cfg) {
  ConfigManager::InitResult result;
  auto clang_path = cwd / ".clang-format";

  try {
    auto clang_content = tooling::generate_clang_format(clang_cfg);
    if (!ConfigManager::write_file(clang_path, clang_content)) {
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

[[nodiscard]] ConfigManager::InitResult write_clang_tidy(const std::filesystem::path& cwd,
                                                         const ConfigManager::InitOptions& opts) {
  ConfigManager::InitResult result;
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
    if (!ConfigManager::write_file(tidy_path, tidy_content)) {
      result.error_message = "Failed to create: " + tidy_path.string();
      return result;
    }
  } catch (const std::exception& error_tidy_path) {
    result.error_message = std::string("Failed to generate .clang-tidy: ") + error_tidy_path.what();
    return result;
  }

  result.success = true;
  return result;
}

[[nodiscard]] ConfigManager::InitResult write_source_files(const std::filesystem::path& cwd,
                                                           const ConfigManager::InitOptions& opts) {
  ConfigManager::InitResult result;

  if (!opts.generate_source) {
    result.success = true;
    return result;
  }

  auto src_dir = cwd / "src";

  try {
    if (!std::filesystem::exists(src_dir)) {
      std::filesystem::create_directories(src_dir);
    }
  } catch (const std::exception& error_src_dir) {
    result.error_message = std::string("Failed to create src/ directory: ") + error_src_dir.what();
    return result;
  }

  auto main_cpp_path = src_dir / "main.cpp";
  constexpr std::string_view main_cpp_content = R"(#include <iostream>

int main() {
    std::cout << "sniffercommit says wello" << std::endl;
    return 0;
}
  )";

  if (!ConfigManager::write_file(main_cpp_path, std::string(main_cpp_content))) {
    result.error_message = "Failed to create " + main_cpp_path.string();
    return result;
  }

  result.src_path = main_cpp_path.string();
  result.success = true;
  return result;
}

[[nodiscard]] ConfigManager::InitResult write_cmake_config(const std::filesystem::path& cwd,
                                                           const ConfigManager::InitOptions& opts,
                                                           const std::string& project_name) {
  ConfigManager::InitResult result;

  if (!opts.enable_cmake) {
    result.success = true;
    return result;
  }

  auto cmake_path = cwd / "CMakeLists.txt";

  try {
    tooling::CMakeConfig cfg;

    cfg.project_name = project_name;
    cfg.version = "0.2.1";
    cfg.cpp_standard = opts.cmake_cpp_standard;
    cfg.target_name = project_name;
    cfg.target_type = opts.cmake_target_type;
    cfg.source_files = {"src/main.cpp"};
    cfg.include_dirs = {"${CMAKE_CURRENT_SOURCE_DIR}/include"};
    cfg.enable_warnings = opts.cmake_enable_warnings;
    cfg.enable_testing = opts.cmake_enable_testing;
    cfg.enable_sanitizers = opts.cmake_enable_sanitizers;
    cfg.enable_install = true;
    cfg.export_compile_commands = true;
    cfg.enable_clang_tidy = opts.enable_clang_tidy;
    cfg.depedencies = opts.depdencies;

    auto cmake_content = tooling::generate_cmake_lists(cfg);

    if (!ConfigManager::write_file(cmake_path, cmake_content)) {
      result.error_message = "Failed to create " + cmake_path.string();
      return result;
    }
  } catch (const std::exception& error_cmake_path) {
    result.error_message =
        std::string("Failed to generate CMakeLists.txt: ") + error_cmake_path.what();
    return result;
  }

  result.cmake_config_path = cmake_path.string();
  result.success = true;
  return result;
}

}  // namespace

ConfigManager::InitResult ConfigManager::initialize(const std::filesystem::path& cwd,
                                                    const InitOptions& opts) {
  InitResult result;

  std::string project_name = opts.project_name;
  if (project_name.empty()) {
    project_name = cwd.filename().string();
  }

  for (const auto& dep : opts.depdencies) {
    if (auto err = dep.validate(); !err.empty()) {
      result.error_message = err;
      return result;
    }
  }

  auto clang_cfg = make_clang_format(opts);
  if (auto err = clang_cfg.validate(); !err.empty()) {
    result.error_message = "Invalid clang-format config: " + err;
    return result;
  }

  auto project_result = write_project_config(cwd, project_name, opts);
  if (!project_result.success) {
    return project_result;
  }

  result.project_config_path = project_result.project_config_path;

  auto format_result = write_clang_format(cwd, clang_cfg);
  if (!format_result.success) {
    return format_result;
  }

  result.tooling_config_path = format_result.tooling_config_path;

  if (opts.enable_clang_tidy) {
    auto tidy_result = write_clang_tidy(cwd, opts);
    if (!tidy_result.success) {
      return tidy_result;
    }
  }

  auto src_result = write_source_files(cwd, opts);
  if (!src_result.success) {
    return src_result;
  }
  result.src_path = src_result.src_path;

  auto cmake_result = write_cmake_config(cwd, opts, project_name);
  if (!cmake_result.success) {
    return cmake_result;
  }
  result.cmake_config_path = cmake_result.cmake_config_path;

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
      result.error_message = "Failed to write Github Action workflow";
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
  try {
    std::string out = util::exec_cmd("git rev-parse --show-toplevel 2>/dev/null");

    if (!out.empty()) {
      std::filesystem::path path(out);

      if (std::filesystem::exists(path)) {
        return path;
      }
    }
  } catch (std::exception& err_git_rev_parse) {
    std::cerr << err_git_rev_parse.what() << "\n";
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
  std::error_code err_code;

  auto parent = path.parent_path();

  if (!parent.empty() && !std::filesystem::exists(parent)) {
    std::filesystem::create_directories(parent, err_code);

    if (err_code) {
      return false;
    }
  }

  auto temp_path = path;
  temp_path += ".tmp";

  {
    std::ofstream out(temp_path, std::ios::trunc);

    if (!out) {
      std::filesystem::remove(temp_path, err_code);
      return false;
    }

    out << content;
    out.flush();

    if (!out.good()) {
      std::filesystem::remove(temp_path, err_code);
      return false;
    }
  }

  // INFO: atomic rename: on POSIX this will atomic
  std::filesystem::rename(temp_path, path, err_code);

  if (err_code) {
    std::filesystem::copy_file(temp_path, path, std::filesystem::copy_options::overwrite_existing,
                               err_code);
    std::filesystem::remove(temp_path, err_code);

    if (err_code) {
      return false;
    }
  }

  return true;
}

}  // namespace sniffercommit
