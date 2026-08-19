#ifndef SNIFFERCOMMIT_APPLICATION_INIT_USE_CASE_HPP
#define SNIFFERCOMMIT_APPLICATION_INIT_USE_CASE_HPP

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "sniffercommit/domain/ports/config_repository.hpp"
#include "sniffercommit/domain/ports/file_system.hpp"

namespace sniffercommit::application {

/**
 * @brief options used to initialized new sniffercommit projects
 */
struct InitOptions {
  std::string project_name;
  std::string style = "Google";
  int indent_width = 2;
  int column_limit = 100;
  std::string pointer_alignment = "Left";
  std::string brace_style = "Attach";
  bool enable_clang_tidy = false;
  std::string tidy_preset = "standard";
  std::string tidy_severity = "error";
  int tidy_header_filter = 1;
  bool enable_cmake = false;
  bool enable_conan = false;
  std::string cmake_cpp_standard = "20";
  std::string cmake_target_type = "executable";
  bool cmake_enable_testing = false;
  bool cmake_enable_sanitizers = false;
  bool cmake_enable_warnings = true;
  bool generate_source = true;
  std::vector<std::string> dependencies;
  bool enable_compiler_checks = false;
  std::string compiler = "g++";
  std::string compiler_cpp_standard = "20";
  std::vector<std::string> compiler_warnings = {"Wall", "Wextra", "Wpedantic"};
  bool compiler_werror = true;
  bool compiler_debug_and_release = false;
};

/**
 * @brief resut of the project intialization process
 *
 * contains execution status with generate file paths or an
 * error message if intialization failed
 */
struct InitResult {
  bool success = false;
  std::string project_config_path;
  std::string tooling_config_path;
  std::string cmake_config_path;
  std::string conan_config_path;
  std::string src_path;
  std::string error_message;
};

/**
 * @brief Application use case reponsible for initialize new project
 */
class InitUseCase {
 public:
  /**
   * @brief ownership of the provided dependencies is transferred to this class
   *
   * @param config_repo Repository used to persists project configuration
   * @param file_system Filesystem abstraction used to create files and directories
   */
  InitUseCase(std::unique_ptr<domain::ports::IConfigRepository> config_repo,
              std::unique_ptr<domain::ports::IFileSystem> file_system);

  /**
   * @brief intialized project in the specified directory
   *
   * generate requested configuration files and optimal project scaffold
   * according to the spullied intialization option
   *
   * @param cwd Target project directory
   * @param opts Initialization options
   *
   * @return InitResult describe outcome of the operation
   */
  [[nodiscard]] InitResult execute(const std::filesystem::path& cwd, const InitOptions& opts);

 private:
  /// Repository used to persist project configuration
  std::unique_ptr<domain::ports::IConfigRepository> config_repo_;

  /// Filesystem abstraction used for all file operations
  std::unique_ptr<domain::ports::IFileSystem> file_system_;
};

}  // namespace sniffercommit::application

#endif
