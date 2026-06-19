#ifndef SNIFFERCOMMIT_APPLICATION_INIT_USE_CASE_HPP
#define SNIFFERCOMMIT_APPLICATION_INIT_USE_CASE_HPP

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "sniffercommit/domain/config.hpp"
#include "sniffercommit/domain/ports/config_repository.hpp"
#include "sniffercommit/domain/ports/file_system.hpp"

namespace sniffercommit::application {

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
  std::string cmake_cpp_standard = "20";
  std::string cmake_target_type = "executable";
  bool cmake_enable_testing = false;
  bool cmake_enable_sanitizers = false;
  bool cmake_enable_warnings = true;
  bool generate_source = true;
  std::vector<std::string> dependencies;
};

struct InitResult {
  bool success = false;
  std::string project_config_path;
  std::string tooling_config_path;
  std::string cmake_config_path;
  std::string src_path;
  std::string error_message;
};

class InitUseCase {
 public:
  InitUseCase(std::unique_ptr<domain::ports::IConfigRepository> config_repo,
              std::unique_ptr<domain::ports::IFileSystem> file_system);

  [[nodiscard]] InitResult execute(const std::filesystem::path& cwd, const InitOptions& opts);

 private:
  std::unique_ptr<domain::ports::IConfigRepository> config_repo_;
  std::unique_ptr<domain::ports::IFileSystem> file_system_;
};

}  // namespace sniffercommit::application

#endif
