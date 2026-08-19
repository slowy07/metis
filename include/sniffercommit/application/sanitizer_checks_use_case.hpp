#ifndef SNIFFERCOMMIT_APPLICATION_SANITIZER_CHECKS_USE_CASE_HPP
#define SNIFFERCOMMIT_APPLICATION_SANITIZER_CHECKS_USE_CASE_HPP

#include <string>
#include <vector>

#include "sniffercommit/domain/config.hpp"
#include "sniffercommit/domain/ports/file_system.hpp"
#include "sniffercommit/domain/ports/shell_executor.hpp"

namespace sniffercommit::application {

class SanitizerChecksUseCase {
 public:
  SanitizerChecksUseCase(std::unique_ptr<domain::ports::IShellExecutor> shell,
                         std::unique_ptr<domain::ports::IFileSystem> fs);

  bool execute(const domain::config::ProjectConfig& cfg, const std::filesystem::path& repo_root,
               bool verbose);

 private:
  bool build_with_sanitizer(const std::filesystem::path& build_dir,
                            const std::string& sanitizer_flag, bool verbose, std::string& output);

  bool run_sanitizer_tests(const std::filesystem::path& build_dir, int timeout, bool verbose,
                           std::string& output);

  std::string to_compiler_flag(const std::string& type);

  std::unique_ptr<domain::ports::IShellExecutor> shell_;
  std::unique_ptr<domain::ports::IFileSystem> fs_;
};

}  // namespace sniffercommit::application

#endif  // !SNIFFERCOMMIT_APPLICATION_SANITIZER_CHECKS_USE_CASE_HPP
