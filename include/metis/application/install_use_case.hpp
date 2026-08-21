#ifndef METIS_APPLICATION_INSTALL_USE_CASE_HPP
#define METIS_APPLICATION_INSTALL_USE_CASE_HPP

#include <filesystem>
#include <memory>
#include <string>

#include "metis/domain/config.hpp"
#include "metis/domain/ports/file_system.hpp"
#include "metis/domain/ports/git_repository.hpp"

namespace metis::application {

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

}  // namespace metis::application

#endif
