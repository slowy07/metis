#include "sniffercommit/application/init_use_case.hpp"

#include <fmt/format.h>

#include <exception>
#include <filesystem>
#include <iostream>
#include <string>

#include "sniffercommit/domain/config.hpp"
#include "sniffercommit/generators/clang_format_generator.hpp"
#include "sniffercommit/generators/clang_tidy_generator.hpp"
#include "sniffercommit/generators/cmake_generator.hpp"

namespace sniffercommit::application {

InitUseCase::InitUseCase(std::unique_ptr<domain::ports::IConfigRepository> config_repo,
                         std::unique_ptr<domain::ports::IFileSystem> file_system)
    : config_repo_(std::move(config_repo)), file_system_(std::move(file_system)) {}

InitResult InitUseCase::execute(const std::filesystem::path& cwd, const InitOptions& opts) {
  InitResult result;
  std::string project_name =
      opts.project_name.empty() ? cwd.filename().string() : opts.project_name;

  // Write .sniffercommit.toml
  auto config_path = cwd / ".sniffercommit.toml";
  std::string config_content;
  std::filesystem::path repo_root = cwd;

  try {
    repo_root = config_repo_->find_git_root();
  } catch (const std::exception&) {
    std::cerr << "use cwd as fallback\n";
  }

  if (opts.enable_clang_tidy) {
    config_content = domain::config::generate_default_config_with_tidy(project_name, opts.style,
                                                                       opts.tidy_preset, repo_root);
  } else {
    config_content = domain::config::generate_default_config(project_name, opts.style, repo_root);
  }

  if (!file_system_->write_file(config_path, config_content)) {
    result.error_message = "Failed to create " + config_path.string();
    return result;
  }
  result.project_config_path = config_path.string();

  // Write .clang-format
  auto clang_path = cwd / ".clang-format";
  try {
    auto clang_content = generators::generate_clang_format(
        opts.style, opts.indent_width, opts.column_limit, opts.pointer_alignment, opts.brace_style);
    if (!file_system_->write_file(clang_path, clang_content)) {
      result.error_message = "Failed to create " + clang_path.string();
      return result;
    }
  } catch (const std::exception& e) {
    result.error_message = std::string("Failed to generate .clang-format: ") + e.what();
    return result;
  }
  result.tooling_config_path = clang_path.string();

  // Write .clang-tidy
  if (opts.enable_clang_tidy) {
    auto tidy_path = cwd / ".clang-tidy";
    try {
      auto tidy_content = generators::generate_clang_tidy(opts.tidy_preset, opts.tidy_severity,
                                                          opts.tidy_header_filter);
      if (!file_system_->write_file(tidy_path, tidy_content)) {
        result.error_message = "Failed to create " + tidy_path.string();
        return result;
      }
    } catch (const std::exception& e) {
      result.error_message = std::string("Failed to generate .clang-tidy: ") + e.what();
      return result;
    }
  }

  // Write src/main.cpp
  if (opts.generate_source) {
    auto src_dir = cwd / "src";
    try {
      if (!file_system_->exists(src_dir)) {
        if (!file_system_->create_directories(src_dir)) {
          result.error_message = "Failed to create src/ directory";
          return result;
        }
      }
    } catch (const std::exception& e) {
      result.error_message = std::string("Failed to create src/ directory: ") + e.what();
      return result;
    }

    auto main_cpp_path = src_dir / "main.cpp";
    constexpr std::string_view main_cpp_content = R"(#include <iostream>

int main() {
    std::cout << "sniffercommit says wello" << std::endl;
    return 0;
}
  )";

    if (!file_system_->write_file(main_cpp_path, std::string(main_cpp_content))) {
      result.error_message = "Failed to create " + main_cpp_path.string();
      return result;
    }
    result.src_path = main_cpp_path.string();
  }

  // Write CMakeLists.txt
  if (opts.enable_cmake) {
    auto cmake_path = cwd / "CMakeLists.txt";
    try {
      auto cmake_content = generators::generate_cmake_lists(
          project_name, opts.cmake_cpp_standard, opts.cmake_target_type, opts.cmake_enable_testing,
          opts.cmake_enable_sanitizers, opts.cmake_enable_warnings, opts.enable_clang_tidy,
          opts.dependencies);
      if (!file_system_->write_file(cmake_path, cmake_content)) {
        result.error_message = "Failed to create " + cmake_path.string();
        return result;
      }
    } catch (const std::exception& e) {
      result.error_message = std::string("Failed to generate CMakeLists.txt: ") + e.what();
      return result;
    }
    result.cmake_config_path = cmake_path.string();
  }

  result.success = true;
  return result;
}

}  // namespace sniffercommit::application
