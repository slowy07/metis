#include "metis/application/perf_checks_use_case.hpp"

#include <fmt/format.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

#include "metis/domain/error_codes.hpp"
#include "metis/domain/ports/shell_executor.hpp"

namespace metis::application {
PerfChecksUseCase::PerfChecksUseCase(std::unique_ptr<domain::ports::IShellExecutor> shell,
                                     std::unique_ptr<domain::ports::IFileSystem> file_system)
  : shell_(std::move(shell))
  , file_system_(std::move(file_system)) {}

PerfResult PerfChecksUseCase::execute(const domain::config::ProjectConfig& cfg,
                                      const std::filesystem::path& repo_root, bool verbose) {
  PerfResult result;

  if (!cfg.perf.enabled) {
    result.output = "[WARN] Performance checks not enabled in config.\n";
    return result;
  }

  auto build_dir = repo_root / cfg.perf.build_dir;

  if (!file_system_->exists(build_dir)) {
    result.success = false;
    result.output = fmt::format(
        "[ERROR] Build Directory does not exists: {}\n"
        "run `cmake -B {} -S . && cmake --build {}` first\n",
        build_dir.string(), cfg.perf.build_dir, cfg.perf.build_dir);
  }

  // INFO: build time checking
  // need some i tweak again after this implement finished
  if (cfg.perf.max_build_time_sec > 0) {
    double build_time = 0.0;
    std::string build_output;
    bool ok = measure_build_time(build_dir, verbose, build_time, build_output);
    result.output += build_output;
    result.build_time_sec = build_time;

    if (!ok) {
      result.success = false;
      result.build_time_ok = false;
      result.output += "[ERROR] Build failed during performance measurement\n";
    } else if (build_time > static_cast<double>(cfg.perf.max_build_time_sec)) {
      result.success = false;
      result.build_time_ok = false;
      result.output += fmt::format("[ERROR] Build time {:.1f}s exceeds threshold {}s\n", build_time,
                                   cfg.perf.max_build_time_sec);
    } else {
      result.output += fmt::format("[INFO] build time: {:.1f}s (threshold: {}s)\n", build_time,
                                   cfg.perf.max_build_time_sec);
    }
  }

  if (!cfg.perf.binary_path.empty() && cfg.perf.max_binary_size_mb > 0) {
    auto binary_full_path = repo_root / cfg.perf.binary_path;
    std::size_t size_bytes = 0;
    std::string size_output;
    bool ok = check_binary_size(binary_full_path, cfg.perf.max_binary_size_mb, verbose, size_bytes,
                                size_output);

    result.output += size_output;
    result.binary_size_bytes = size_bytes;

    if (!ok) {
      result.success = false;
      result.binary_size_ok = false;
    } else {
      double size_mb = static_cast<double>(size_bytes) / (1024.0 * 1024.0);
      result.output += fmt::format("[INFO] Binary size: {:.2f} MiB (threshold: {} MiB)\n", size_mb,
                                   cfg.perf.max_binary_size_mb);
    }
  }

  if (!cfg.perf.benchmark_regex.empty()) {
    std::string bench_output;
    bool ok = run_benchmarks(build_dir, cfg.perf.benchmark_regex, verbose, bench_output);
    result.output += bench_output;

    if (!ok) {
      result.success = false;
      result.benchmark_ok = false;
      result.output += "[ERROR] Benchmark execution failed\n";
    } else {
      result.output += "[INFO] Benchmarks completed successfully\n";
    }
  }

  return result;
}

bool PerfChecksUseCase::measure_build_time(const std::filesystem::path& build_dir, bool verbose,
                                           double& out_seconds, std::string& output) {
  std::string cmd = "cmake --build " + build_dir.string() + " --clean-first";

  if (verbose) {
    output += fmt::format("$ {}\n", cmd);
  }

  auto start = std::chrono::steady_clock::now();
  auto result = shell_->exec_captured(cmd);
  auto end = std::chrono::steady_clock::now();

  output += result.output_;

  std::chrono::duration<double> elapsed = end - start;
  out_seconds = elapsed.count();
  return result.exit_code_ == 0;
}

bool PerfChecksUseCase::check_binary_size(const std::filesystem::path& binary_path,
                                          std::size_t max_size_mb, bool verbose,
                                          std::size_t& out_bytes, std::string& output) {
  if (!file_system_->exists(binary_path)) {
    output = fmt::format("[ERROR] binary not found: {}\n", binary_path.string());
    return false;
  }

  try {
    out_bytes = static_cast<std::size_t>(std::filesystem::file_size(binary_path));
  } catch (const std::exception& error) {
    output = fmt::format("[ERROR] Cannot read binary size: {}\n", error.what());
    return false;
  }

  double size_mb = static_cast<double>(out_bytes) / (1024.0 * 1024.0);

  if (verbose) {
    output = fmt::format("[INFO] Binary: {} → {:.2f} MiB\n", binary_path.string(), size_mb);
  }

  if (out_bytes > max_size_mb * 1024 * 1024) {
    output += fmt::format("[ERROR] Binary size {:.2f} MiB exceeds threshold {} MiB\n", size_mb,
                          max_size_mb);
    return false;
  }

  return true;
}

bool PerfChecksUseCase::run_benchmarks(const std::filesystem::path& build_dir,
                                       const std::string& regex, bool verbose,
                                       std::string& output) {
  std::string cmd = "ctest --test-dir " + build_dir.string() + " -R \"" + regex + "\"";
  cmd += " --output-on-failure";

  if (verbose) {
    output += fmt::format("$ {}\n", cmd);
  }

  auto result = shell_->exec_captured(cmd);
  output += result.output_;
  return result.exit_code_ == 0;
}

}  // namespace metis::application
