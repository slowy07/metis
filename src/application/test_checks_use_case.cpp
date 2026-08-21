#include "metis/application/test_checks_use_case.hpp"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <ranges>
#include <regex>
#include <sstream>
#include <string>

#include "fmt/format.h"
#include "metis/domain/error_codes.hpp"
#include "metis/util.hpp"

namespace metis::application {
TestChecksUseCase::TestChecksUseCase(std::unique_ptr<domain::ports::IShellExecutor> shell,
                                     std::unique_ptr<domain::ports::IFileSystem> fs)
    : shell_(std::move(shell)), fs_(std::move(fs)) {}

TestResult TestChecksUseCase::execute(const domain::config::ProjectConfig& cfg,
                                      const std::filesystem::path& repo_root, bool coverage,
                                      bool verbose) {
  TestResult result;

  auto build_dir = repo_root / cfg.test.build_dir;

  if (!fs_->exists(build_dir)) {
    result.output = fmt::format(
        "[ERROR] Build Directory does not exists: {}\n"
        "Run `cmake -B {} -S . && cmake --build {}` first\n",
        build_dir.string(), cfg.test.build_dir, cfg.test.build_dir);

    return result;
  }

  result = run_ctest(build_dir, verbose, cfg.test.timeout);

  if (!result.success) {
    return result;
  }

  if (coverage) {
    auto coverage_result = run_coverage(build_dir, verbose);

    result.line_coverage = coverage_result.line_coverage;
    result.branch_coverage = coverage_result.branch_coverage;
    result.function_coverage = coverage_result.function_coverage;
    result.output += coverage_result.output;

    if (cfg.test.line_threshold > 0.0 && result.line_coverage >= 0.0 &&
        result.line_coverage < cfg.test.line_threshold) {
      result.coverage_ok = false;
      result.output += fmt::format("[ERROR] Line Coverage {:.1f}% is below threshold {:.1f}%\n",
                                   result.line_coverage, cfg.test.line_threshold);
    }

    if (cfg.test.branch_threshold > 0.0 && result.branch_coverage >= 0.0 &&
        result.branch_coverage < cfg.test.branch_threshold) {
      result.coverage_ok = false;
      result.output += fmt::format("[ERROR] Branch coverage {:.1f}% is below threshold {:.1f}%\n",
                                   result.branch_coverage, cfg.test.branch_threshold);
    }

    if (cfg.test.function_threshold > 0.0 && result.function_coverage >= 0.0 &&
        result.function_coverage < cfg.test.function_threshold) {
      result.coverage_ok = false;
      result.output += fmt::format("[ERROR] Function coverage {:.1f}% is below threshold {:.1f}%\n",
                                   result.function_coverage, cfg.test.function_threshold);
    }
  }

  return result;
}

TestResult TestChecksUseCase::run_ctest(const std::filesystem::path& build_dir, bool verbose,
                                        int timeout) {
  TestResult result;

  std::string cmd = "ctest --test-dir " + build_dir.string();

  if (verbose) {
    cmd += " -V";
  }

  if (timeout > 0) {
    cmd += " --timeout " + std::to_string(timeout);
  }

  cmd += " --output-on-failure";
  if (verbose) {
    result.output += fmt::format("$ {}\n", cmd);
  }

  auto exec_result = shell_->exec_captured(cmd);
  result.output += exec_result.output_;

  std::regex tests_passed_re("Tests Passed:\\s*(\\d+)");
  std::regex tests_failed_re("Tests Failed:\\s*(\\d+)");
  std::smatch match;

  if (std::regex_search(exec_result.output_, match, tests_passed_re) && match.size() > 1) {
    result.total_count =
        std::stoi(match[1].str()) + (result.failed_count > 0 ? result.failed_count : 0);
  }

  if (std::regex_search(exec_result.output_, match, tests_failed_re) && match.size() > 1) {
    result.failed_count = std::stoi(match[1].str());
    result.success = (result.failed_count == 0);
  } else {
    result.success = (exec_result.exit_code_ == 0);
  }

  return result;
}

TestResult TestChecksUseCase::run_coverage(const std::filesystem::path& build_dir, bool verbose) {
  TestResult result;

  std::string cmd = "ctest --test-dir " + build_dir.string() + " -T Coverage";

  if (verbose) {
    result.output += fmt::format("$ {}\n", cmd);
  }

  auto exec_result = shell_->exec_captured(cmd);
  result.output += exec_result.output_;

  std::filesystem::path coverage_file;

  for (const auto& entry : std::filesystem::recursive_directory_iterator(build_dir)) {
    if (entry.is_regular_file() && entry.path().extension() == ".xml" &&
        entry.path().filename().string().find("coverage") != std::string::npos) {
      coverage_file = entry.path();
      break;
    }
  }

  if (coverage_file.empty()) {
    result.output += "[WARN] No coverage XML file found in build directory.\n";
    return result;
  }

  if (verbose) {
    result.output += fmt::format("[INFO] Using coverage file: {}\n", coverage_file.string());
  }

  std::ifstream file(coverage_file);
  if (!file.is_open()) {
    result.output += fmt::format("[ERROR] Cannot open coverage file: {}\n", coverage_file.string());

    return result;
  }

  std::stringstream buffer;
  buffer << file.rdbuf();
  std::string xml_content = buffer.str();

  parse_coverage_xml(xml_content, result.line_coverage, result.branch_coverage,
                     result.function_coverage);

  if (result.line_coverage >= 0.0) {
    result.output += fmt::format("[INFO] Line Coverage: {:.1f}%\n", result.line_coverage);
  }

  if (result.branch_coverage >= 0.0) {
    result.output += fmt::format("[INFO] Branch Coverage: {:.1f}%\n", result.branch_coverage);
  }

  if (result.function_coverage >= 0.0) {
    result.output += fmt::format("[INFO] Function coverage: {:.1f}%\n", result.function_coverage);
  }

  return result;
}

void TestChecksUseCase::parse_coverage_xml(const std::string& xml_content, double& line_coverage,
                                           double& branch_coverage, double& function_coverage) {
  line_coverage = -1.0;
  branch_coverage = -1.0;
  function_coverage = -1.0;

  std::regex line_rate_re("line-rate=\"([0-9.]+)\"");
  std::regex branch_rate_re("branch-rate=\"([0-9.]+)\"");
  std::smatch match;

  if (std::regex_search(xml_content, match, line_rate_re) && match.size() > 1) {
    line_coverage = std::stod(match[1].str()) * 100.0;
  }

  if (std::regex_search(xml_content, match, branch_rate_re) && match.size() > 1) {
    branch_coverage = std::stod(match[1].str()) * 100.0;
  }
}

}  // namespace metis::application
