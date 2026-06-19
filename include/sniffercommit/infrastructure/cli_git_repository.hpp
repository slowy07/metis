#ifndef SNIFFERCOMMIT_INFRASTRUCTURE_CLI_GIT_REPOSITORY_HPP
#define SNIFFERCOMMIT_INFRASTRUCTURE_CLI_GIT_REPOSITORY_HPP

#include <filesystem>
#include <memory>

#include "sniffercommit/domain/ports/git_repository.hpp"
#include "sniffercommit/domain/ports/shell_executor.hpp"

namespace sniffercommit::infrastructure {

class CliGitRepository : public domain::ports::IGitRepository {
 public:
  explicit CliGitRepository(std::unique_ptr<domain::ports::IShellExecutor> shell);

  [[nodiscard]] std::vector<std::string> list_staged_files(
      const std::filesystem::path& repo_root) override;
  [[nodiscard]] std::vector<std::string> list_all_files(
      const std::filesystem::path& repo_root) override;
  [[nodiscard]] bool is_file_modified(const std::filesystem::path& file) override;
  [[nodiscard]] std::filesystem::path find_repo_root(const std::filesystem::path& start) override;

 private:
  std::unique_ptr<domain::ports::IShellExecutor> shell_;
};

}  // namespace sniffercommit::infrastructure

#endif
