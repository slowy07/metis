#ifndef METIS_APPLICATION_TEST_CHECKS_USE_CASE_HPP
#define METIS_APPLICATION_TEST_CHECKS_USE_CASE_HPP

#include <string>

#include "metis/domain/config.hpp"
#include "metis/domain/ports/file_system.hpp"
#include "metis/domain/ports/shell_executor.hpp"

namespace metis::application {

struct TestResult {
  bool success = false;
  bool coverage_ok = true;
  int failed_count = 0;
  int total_count = 0;
  double line_coverage = -1.0;
  double branch_coverage = -1.0;
  double function_coverage = -1.0;
  std::string output;
};

class TestChecksUseCase {
 public:
  TestChecksUseCase(std::unique_ptr<domain::ports::IShellExecutor> shell,
                    std::unique_ptr<domain::ports::IFileSystem> fs);
  TestResult execute(const domain::config::ProjectConfig& cfg,
                     const std::filesystem::path& repo_root, bool coverage, bool verbose);

 private:
  TestResult run_ctest(const std::filesystem::path& build_dir, bool verbose, int timeout);
  TestResult run_coverage(const std::filesystem::path& build_dir, bool verbose);
  void parse_coverage_xml(const std::string& xml_content, double& line_coverage,
                          double& branch_coverage, double& function_coverage);

  std::unique_ptr<domain::ports::IShellExecutor> shell_;
  std::unique_ptr<domain::ports::IFileSystem> fs_;
};

}  // namespace metis::application

#endif  // !METIS_APPLICATION_TEST_CHECKS_USE_CASE_HPP
