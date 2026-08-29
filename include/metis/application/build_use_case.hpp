#ifndef METIS_APPLICATION_BUILD_USE_CASE_HPP
#define METIS_APPLICATION_BUILD_USE_CASE_HPP

#include <memory>
#include <string>

#include "metis/domain/ports/file_system.hpp"
#include "metis/domain/ports/shell_executor.hpp"

namespace metis::application {
struct BuildResult {
  bool success = false;
  std::string output;
  double configure_time_sec = 0.0;
  double build_time_sec = 0.0;
};

class BuildUseCase {
 public:
  BuildUseCase(std::unique_ptr<domain::ports::IShellExecutor> shell,
               std::unique_ptr<domain::ports::IFileSystem> file_system);

  BuildResult execute(const std::filesystem::path& repo_root, const std::string& build_dir,
                      bool clean, bool verbose, int jobs);

 private:
  std::unique_ptr<domain::ports::IShellExecutor> shell_;
  std::unique_ptr<domain::ports::IFileSystem> file_system_;
};
}  // namespace metis::application

#endif  // !METIS_APPLICATION_BUILD_USE_CASE_HPP
