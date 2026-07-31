#ifndef SNIFFERCOMMIT_DOMAIN_PORTS_GIT_REPOSITORY_HPP
#define SNIFFERCOMMIT_DOMAIN_PORTS_GIT_REPOSITORY_HPP

#include <filesystem>
#include <string>
#include <vector>

namespace sniffercommit::domain::ports {

// Interface for git repository operations.
// lazy: only one implementation (CliGitRepository). Same pattern.
struct IGitRepository {
  virtual ~IGitRepository() = default;

  [[nodiscard]] virtual std::vector<std::string> list_staged_files(
      const std::filesystem::path& repo_root) = 0;
  [[nodiscard]] virtual std::vector<std::string> list_all_files(
      const std::filesystem::path& repo_root) = 0;
  [[nodiscard]] virtual std::filesystem::path find_repo_root(
      const std::filesystem::path& start) = 0;
};

}  // namespace sniffercommit::domain::ports

#endif
