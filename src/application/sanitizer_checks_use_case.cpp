#include "metis/application/sanitizer_checks_use_case.hpp"

#include <fmt/format.h>

#include <algorithm>
#include <iostream>
#include <string>
#include <utility>

#include "metis/domain/error_codes.hpp"
#include "metis/domain/ports/file_system.hpp"
#include "metis/domain/ports/shell_executor.hpp"

namespace metis::application {
SanitizerChecksUseCase::SanitizerChecksUseCase(std::unique_ptr<domain::ports::IShellExecutor> shell,
                                               std::unique_ptr<domain::ports::IFileSystem> file_system)
    : shell_(std::move(shell)), file_system_(std::move(file_system)) {}

std::string SanitizerChecksUseCase::to_compiler_flag(const std::string& type) {
  if (type == "address") {
    return "-fsanitize=address";
  }

  if (type == "undefined") {
    return "-fsanitize=undefined";
  }

  if (type == "thread") {
    return "-fsanitize=thread";
  }

  if (type == "leak") {
    return "-fsanitize=leak";
  }

  return "";
}

bool SanitizerChecksUseCase::execute(const domain::config::ProjectConfig& cfg,
                                     const std::filesystem::path& repo_root, bool verbose) {
  if (cfg.sanitizer.types.empty()) {
    return true;
  }

  auto build_dir = repo_root / cfg.sanitizer.build_dir;

  if (!file_system_->exists(build_dir)) {
    std::cerr << fmt::format("[ERROR] Build directory does not exists: {}\n", build_dir.string());
    return false;
  }

  bool all_passed = true;

  for (const auto& type : cfg.sanitizer.types) {
    std::string flag = to_compiler_flag(type);

    if (flag.empty()) {
      std::cerr << fmt::format(
          "[ERROR] Unknown sanitizer type: {}. Supported: address, undefined, thread, leak\n",
          type);
      all_passed = false;
      continue;
    }

    std::cerr << fmt::format("[INFO] Running {} sanitizer ...\n", type);
    std::string build_output;

    bool build_ok = build_with_sanitizer(build_dir, flag, verbose, build_output);

    if (!build_ok) {
      std::cerr << "[ERROR] build with " << type << " sanitizer failed \n";
      if (verbose) {
        std::cerr << build_output << "\n";
      }

      all_passed = false;
      continue;
    }

    std::string test_output;
    bool test_ok = run_sanitizer_tests(build_dir, cfg.sanitizer.timeout, verbose, test_output);

    if (!test_ok) {
      std::cerr << "[ERROR] Test with " << type << " sanitizer failed \n";
      if (verbose) {
        std::cerr << test_output << "\n";
      }

      all_passed = false;
      continue;
    }

    std::cerr << fmt::format("[INFO] {} sanitizer: Passed\n", type);
  }

  return all_passed;
}

bool SanitizerChecksUseCase::build_with_sanitizer(const std::filesystem::path& build_dir,
                                                  const std::string& sanitizer_flag, bool verbose,
                                                  std::string& output) {
  std::string cmd =
      "cmake --build " + build_dir.string() + " -- -DCMAKE_CXX_FLAGS=\"" + sanitizer_flag + "\"";

  if (verbose) {
    output = fmt::format("$ {}\n", cmd);
  }

  auto result = shell_->exec_captured(cmd);
  output += result.output_;
  return result.exit_code_ == 0;
}

bool SanitizerChecksUseCase::run_sanitizer_tests(const std::filesystem::path& build_dir,
                                                 int timeout, bool verbose, std::string& output) {
  std::string cmd = "ctest --test-dir " + build_dir.string() + " --output=on-failure";

  if (timeout > 0) {
    cmd += " --timeout " + std::to_string(timeout);
  }

  if (verbose) {
    output += fmt::format("$ {}\n", cmd);
  }

  auto result = shell_->exec_captured(cmd);
  output += result.output_;
  return result.exit_code_ == 0;
}

}  // namespace metis::application
