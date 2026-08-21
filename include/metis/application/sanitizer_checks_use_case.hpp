#ifndef METIS_APPLICATION_SANITIZER_CHECKS_USE_CASE_HPP
#define METIS_APPLICATION_SANITIZER_CHECKS_USE_CASE_HPP

#include <string>
#include <vector>

#include "metis/domain/config.hpp"
#include "metis/domain/ports/file_system.hpp"
#include "metis/domain/ports/shell_executor.hpp"

namespace metis::application {

class SanitizerChecksUseCase {
 public:
  SanitizerChecksUseCase(std::unique_ptr<domain::ports::IShellExecutor> shell,
                         std::unique_ptr<domain::ports::IFileSystem> file_system);

  bool execute(const domain::config::ProjectConfig& cfg, const std::filesystem::path& repo_root,
               bool verbose);

 private:
  bool build_with_sanitizer(const std::filesystem::path& build_dir,
                            const std::string& sanitizer_flag, bool verbose, std::string& output);

  bool run_sanitizer_tests(const std::filesystem::path& build_dir, int timeout, bool verbose,
                           std::string& output);

  static std::string to_compiler_flag(const std::string& type);

  std::unique_ptr<domain::ports::IShellExecutor> shell_;
  std::unique_ptr<domain::ports::IFileSystem> file_system_;
};

}  // namespace metis::application

#endif  // !METIS_APPLICATION_SANITIZER_CHECKS_USE_CASE_HPP
