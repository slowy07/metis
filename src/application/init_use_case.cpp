#include "metis/application/init_use_case.hpp"

#include <fmt/format.h>

#include <exception>
#include <filesystem>
#include <iostream>
#include <string>

#include "metis/domain/config.hpp"
#include "metis/generators/clang_format_generator.hpp"
#include "metis/generators/clang_tidy_generator.hpp"
#include "metis/generators/cmake_generator.hpp"
#include "metis/generators/conan_generator.hpp"

namespace metis::application {

// Creates an InitUseCase with ownership of its dependencies.
// config_repo is used for find_git_root() to resolve relative paths.
// file_system is used for all file writes (config, .clang-format, etc.)
InitUseCase::InitUseCase(std::unique_ptr<infrastructure::TomlConfigRepository> config_repo,
                         std::unique_ptr<domain::ports::IFileSystem> file_system)
  : config_repo_(std::move(config_repo))
  , file_system_(std::move(file_system)) {}

// Main entry point for project initialization.
//
// Generates configuration files based on InitOptions:
//   1. .metis.toml  — always (main config)
//   2. .clang-format        — always (formatter config)
//   3. .clang-tidy          — if --enable-clang-tidy
//   4. src/main.cpp         — if --generate-src (default with --enable-cmake)
//   5. CMakeLists.txt       — if --enable-cmake
//   6. conanfile.py         — if --enable-conan (requires --enable-cmake)
//
// Each file write is independent; failure on one doesn't prevent others.
// The result struct accumulates paths and any error messages.
namespace {

// Writes content to path; on failure fills result.error_message.
bool write_or_fail(metis::domain::ports::IFileSystem& fs, InitResult& result,
                   const std::filesystem::path& path, const std::string& content) {
  if (fs.write_file(path, content)) {
    return true;
  }
  result.error_message = "Failed to create " + path.string();
  return false;
}

// Generates content via make_content and writes it; generator exceptions and
// write failures both land in result.error_message.
template <typename Make>
bool generate_and_write(metis::domain::ports::IFileSystem& fs, InitResult& result,
                        const std::filesystem::path& path, const char* what,
                        const Make& make_content) {
  try {
    return write_or_fail(fs, result, path, make_content());
  } catch (const std::exception& e) {
    result.error_message = std::string("Failed to generate ") + what + ": " + e.what();
    return false;
  }
}

}  // namespace

InitResult InitUseCase::execute(const std::filesystem::path& cwd, const InitOptions& opts) {
  InitResult result;
  std::string project_name =
      opts.project_name.empty() ? cwd.filename().string() : opts.project_name;

  // Write .metis.toml
  auto config_path = cwd / ".metis.toml";
  std::string config_content;
  // Resolve repo root for relative path references in config.
  // e.g. .clang-tidy path in the clang-tidy check args.
  // Falls back to cwd if not in a git repo.
  std::filesystem::path repo_root = cwd;

  try {
    repo_root = config_repo_->find_git_root();
  } catch (const std::exception&) {  // NOLINT: fallback to cwd if not in a git repo
  }

  if (opts.enable_clang_tidy) {
    config_content = domain::config::generate_default_config_with_tidy(project_name, opts.style,
                                                                       opts.tidy_preset, repo_root);
  } else {
    config_content = domain::config::generate_default_config(project_name, opts.style, repo_root);
  }

  if (opts.enable_compiler_checks) {
    config_content += domain::config::generate_compiler_checks(
        opts.compiler, opts.compiler_cpp_standard, opts.compiler_warnings, opts.compiler_werror,
        opts.compiler_debug_and_release);
  }

  if (opts.enable_security_checks) {
    config_content += domain::config::generate_security_checks_config();
  }

  if (!write_or_fail(*file_system_, result, config_path, config_content)) {
    return result;
  }
  result.project_config_path = config_path.string();

  // Write .clang-format
  auto clang_path = cwd / ".clang-format";
  const auto clang_ok = generate_and_write(*file_system_, result, clang_path, ".clang-format", [&] {
    return generators::generate_clang_format(opts.style, opts.indent_width, opts.column_limit,
                                             opts.pointer_alignment, opts.brace_style);
  });
  if (!clang_ok) {
    return result;
  }
  result.tooling_config_path = clang_path.string();

  // Write .clang-tidy
  if (opts.enable_clang_tidy) {
    const auto tidy_path = cwd / ".clang-tidy";
    if (!generate_and_write(*file_system_, result, tidy_path, ".clang-tidy", [&] {
          return generators::generate_clang_tidy(opts.tidy_preset, opts.tidy_severity,
                                                 opts.tidy_header_filter);
        })) {
      return result;
    }
  }

  // Write src/main.cpp
  if (opts.generate_source) {
    const auto src_dir = cwd / "src";
    try {
      if (!file_system_->exists(src_dir) && !file_system_->create_directories(src_dir)) {
        result.error_message = "Failed to create src/ directory";
        return result;
      }
    } catch (const std::exception& e) {
      result.error_message = std::string("Failed to create src/ directory: ") + e.what();
      return result;
    }

    auto main_cpp_path = src_dir / "main.cpp";
    constexpr std::string_view main_cpp_content = R"(#include <iostream>

int main() {
    std::cout << "metis says wello" << std::endl;
    return 0;
}
  )";

    if (!write_or_fail(*file_system_, result, main_cpp_path, std::string(main_cpp_content))) {
      return result;
    }
    result.src_path = main_cpp_path.string();
  }

  // Write CMakeLists.txt
  if (opts.enable_cmake) {
    const auto cmake_path = cwd / "CMakeLists.txt";
    if (!generate_and_write(*file_system_, result, cmake_path, "CMakeLists.txt", [&] {
          return generators::generate_cmake_lists(
              project_name, opts.cmake_cpp_standard, opts.cmake_target_type,
              opts.cmake_enable_testing, opts.cmake_enable_sanitizers, opts.cmake_enable_warnings,
              opts.enable_clang_tidy, opts.enable_conan, opts.dependencies);
        })) {
      return result;
    }
    result.cmake_config_path = cmake_path.string();
  }

  // Write conanfile.py
  if (opts.enable_conan) {
    const auto conan_path = cwd / "conanfile.py";
    if (!generate_and_write(*file_system_, result, conan_path, "conanfile.py", [&] {
          return generators::generate_conanfile(project_name, opts.cmake_enable_testing,
                                                opts.dependencies);
        })) {
      return result;
    }
    result.conan_config_path = conan_path.string();
  }

  result.success = true;
  return result;
}

}  // namespace metis::application
