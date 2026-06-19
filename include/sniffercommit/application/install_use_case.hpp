#ifndef SNIFFERCOMMIT_APPLICATION_INSTALL_USE_CASE_HPP
#define SNIFFERCOMMIT_APPLICATION_INSTALL_USE_CASE_HPP

#include <filesystem>
#include <memory>
#include <string>

#include "sniffercommit/domain/config.hpp"
#include "sniffercommit/domain/ports/file_system.hpp"
#include "sniffercommit/domain/ports/git_repository.hpp"

namespace sniffercommit::application {

struct InstallResult {
  bool hook_installed = false;
  bool workflow_installed = false;
  std::string hook_path;
  std::string workflow_path;
  std::string error_message;
};

class InstallUseCase {
 public:
  InstallUseCase(std::unique_ptr<domain::ports::IFileSystem> file_system,
                 std::unique_ptr<domain::ports::IGitRepository> git_repo);

  [[nodiscard]] InstallResult execute(const std::filesystem::path& repo_root,
                                      const domain::config::ProjectConfig& cfg);

 private:
  std::unique_ptr<domain::ports::IFileSystem> file_system_;
  std::unique_ptr<domain::ports::IGitRepository> git_repo_;
};

}  // namespace sniffercommit::application

#endif
