#ifndef METIS_APPLICATION_PERF_CHECKS_USE_CASE_HPP
#define METIS_APPLICATION_PERF_CHECKS_USE_CASE_HPP

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>

#include "metis/domain/config.hpp"
#include "metis/domain/ports/file_system.hpp"
#include "metis/domain/ports/shell_executor.hpp"

namespace metis::application {
struct PerfResult {
  bool success = true;
  bool binary_size_ok = true;
  bool build_time_ok = true;
  bool benchmark_ok = true;
  std::size_t binary_size_bytes = 0;
  double build_time_sec = 0.0;
  std::string output;
};

class PerfChecksUseCase {
 public:
  PerfChecksUseCase(std::unique_ptr<domain::ports::IShellExecutor> shell,
                    std::unique_ptr<domain::ports::IFileSystem> file_system);

  PerfResult execute(const domain::config::ProjectConfig& cfg, const std::filesystem::path& repo_root, bool verbose);

 private:
  bool measure_build_time(const std::filesystem::path& build_dir, bool verbose, double& out_seconds,
                          std::string& output);

  bool check_binary_size(const std::filesystem::path& binary_path, std::size_t max_size_mb,
                         bool verbose, std::size_t& out_bytes, std::string& output);

  bool run_benchmarks(const std::filesystem::path& build_dir, const std::string& regex,
                      bool verbose, std::string& output);

  std::unique_ptr<domain::ports::IShellExecutor> shell_;
  std::unique_ptr<domain::ports::IFileSystem> file_system_;
};
}  // namespace metis::application

#endif  // !METIS_APPLICATION_PERF_CHECKS_USE_CASE_HPP
