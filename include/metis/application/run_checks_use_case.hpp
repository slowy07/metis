#ifndef METIS_APPLICATION_RUN_CHECKS_USE_CASE_HPP
#define METIS_APPLICATION_RUN_CHECKS_USE_CASE_HPP

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "metis/application/checks/shell_check.hpp"
#include "metis/domain/config.hpp"
#include "metis/domain/ports/file_system.hpp"
#include "metis/domain/ports/git_repository.hpp"
#include "metis/domain/ports/shell_executor.hpp"

namespace metis::infrastructure {
class CheckCache;
}

namespace metis::application {

enum class FileSource : std::uint8_t {
  STAGED,
  ALL_REPO,
  EXPLICIT,
};

enum class RunMode : std::uint8_t {
  CHECK,
  FORMAT,
};

struct RunOptions {
  FileSource source = FileSource::STAGED;
  std::vector<std::string> explicit_files;
  bool verbose = false;
  bool dry_run = false;
  RunMode mode = RunMode::CHECK;
};

// Maps a config check to its concrete implementation by command basename
// (clang-format/clang-tidy/compilers/cmake/git/...; else custom shell command).
// Shared by RunChecksUseCase and `metis sync` environment validation.
[[nodiscard]] std::unique_ptr<domain::Check> make_check(const domain::config::Check& config);

class RunChecksUseCase {
 public:
  RunChecksUseCase(std::unique_ptr<domain::ports::IShellExecutor> shell,
                   std::unique_ptr<domain::ports::IGitRepository> git_repo,
                   std::unique_ptr<domain::ports::IFileSystem> file_system);

  void set_cache(infrastructure::CheckCache* cache) { cache_ = cache; }

  [[nodiscard]] int execute(const domain::config::ProjectConfig& cfg, const RunOptions& opts);

 private:
  std::vector<std::string> collect_files(const std::filesystem::path& repo_root,
                                         const RunOptions& opts,
                                         const std::vector<std::string>& exclude_paths);
  int execute_checks(const std::filesystem::path& repo_root,
                     const domain::config::ProjectConfig& cfg,
                     const std::vector<std::string>& files, const RunOptions& opts);
  int execute_format(const std::filesystem::path& repo_root, const std::vector<std::string>& files,
                     const RunOptions& opts);

  std::unique_ptr<domain::ports::IShellExecutor> shell_;
  std::unique_ptr<domain::ports::IGitRepository> git_repo_;
  std::unique_ptr<domain::ports::IFileSystem> file_system_;
  infrastructure::CheckCache* cache_ = nullptr;
};

}  // namespace metis::application

#endif
