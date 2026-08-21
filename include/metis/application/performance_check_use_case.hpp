#ifndef METIS_APPLICATION_PERFORMANCE_CHECK_USE_CASE_HPP
#define METIS_APPLICATION_PERFORMANCE_CHECK_USE_CASE_HPP

#include <memory>
#include <string>

#include "metis/domain/performance.hpp"
#include "metis/domain/ports/file_system.hpp"
#include "metis/domain/ports/shell_executor.hpp"

namespace metis::application {
struct PerformanceCheckOptions {
  std::string build_dir = "build";
  std::string benchmark_exe;
  bool set_baseline = false;
  double regression_threshold_pct = 5.0;
  bool verbose = false;
};

class PerformanceCheckUseCase {
 public:
  PerformanceCheckUseCase(std::unique_ptr<domain::ports::IShellExecutor> shell,
                          std::unique_ptr<domain::ports::IFileSystem> fs);

  [[nodiscard]] domain::PerformanceCheckResult execute(const std::filesystem::path& repo_root,
                                                       const PerformanceCheckOptions& opts);

 private:
  [[nodiscard]] domain::PerformanceMetric measure_binary_size(
      const std::filesystem::path& repo_root, const std::string& build_dir) const;
  [[nodiscard]] domain::PerformanceMetric measure_compile_time(
      const std::filesystem::path& repo_root, const std::string& build_dir, bool verbose) const;
  [[nodiscard]] domain::PerformanceMetric measure_benchmark(const std::filesystem::path& repo_root,
                                                            const std::string& benchmark_exe,
                                                            bool verbose) const;

  [[nodiscard]] std::filesystem::path baseline_path(const std::filesystem::path& repo_root) const;
  [[nodiscard]] std::vector<domain::PerformanceMetric> load_baseline(
      const std::filesystem::path& repo_root) const;
  void save_baseline(const std::filesystem::path& repo_root,
                     const std::vector<domain::PerformanceMetric>& metrics) const;

  void detect_regressions(const std::vector<domain::PerformanceMetric>& current,
                          const std::vector<domain::PerformanceMetric>& baseline,
                          double threshold_pct, domain::PerformanceCheckResult& out) const;

  std::unique_ptr<domain::ports::IShellExecutor> shell_;
  std::unique_ptr<domain::ports::IFileSystem> fs_;
};
}  // namespace metis::application

#endif  // !METIS_APPLICATION_PERFORMANCE_CHECK_USE_CASE_HPP
