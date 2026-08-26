#include "metis/application/build_use_case.hpp"

#include <fmt/format.h>

#include <algorithm>
#include <chrono>
#include <memory>
#include <string>
#include <utility>

#include "metis/domain/ports/file_system.hpp"
#include "metis/domain/ports/shell_executor.hpp"
#include "metis/util.hpp"

namespace metis::application {

BuildUseCase::BuildUseCase(std::unique_ptr<domain::ports::IShellExecutor> shell,
                           std::unique_ptr<domain::ports::IFileSystem> file_system)
  : shell_(std::move(shell))
  , file_system_(std::move(file_system)) {}

BuildResult BuildUseCase::execute(const std::filesystem::path& repo_root,
                                  const std::string& build_dir, bool clean, bool verbose,
                                  int jobs) {
  BuildResult result;
  auto full_build_dir = repo_root / build_dir;
  auto cmake_lists = repo_root / "CMakeLists.txt";

  if (!file_system_->exists(cmake_lists)) {
    result.output = fmt::format("[ERROR] No CMakeLists.txt found in {}\n", repo_root.string());

    result.success = false;
    return result;
  }

  {
    auto start = std::chrono::steady_clock::now();
    std::string cmd = "cmake -B " + util::shell_escape(full_build_dir.string()) + " -S " +
                      util::shell_escape(repo_root.string());

    if (verbose) {
      result.output += fmt::format("$ {}\n", cmd);
    }

    auto exec_result = shell_->exec_captured(cmd);
    result.output += exec_result.output_;

    if (exec_result.exit_code_ != 0) {
      result.output += "\n[ERROR] CMake configuration failed\n";
      result.success = false;
      return result;
    }

    auto end = std::chrono::steady_clock::now();
    result.configure_time_sec = std::chrono::duration<double>(end - start).count();
  }

  {
    auto start = std::chrono::steady_clock::now();
    std::string cmd = "cmake --build " + util::shell_escape(full_build_dir.string());

    if (clean) {
      cmd += " --clean-first";
    }

    if (jobs > 0) {
      cmd += " --parallel " + std::to_string(jobs);
    }

    if (verbose) {
      result.output += fmt::format("$ {}\n", cmd);
    }

    auto exec_result = shell_->exec_captured(cmd);
    result.output += exec_result.output_;

    if (exec_result.exit_code_ != 0) {
      result.output += "\n[ERROR] Build failed\n";
      result.success = false;
      return result;
    }

    auto end = std::chrono::steady_clock::now();
    result.build_time_sec = std::chrono::duration<double>(end - start).count();
  }

  result.success = true;
  return result;
}

}  // namespace metis::application
