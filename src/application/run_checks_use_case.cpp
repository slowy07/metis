#include "sniffercommit/application/run_checks_use_case.hpp"

#include <fmt/format.h>

#include <algorithm>
#include <exception>
#include <filesystem>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include "sniffercommit/application/checks/build_check.hpp"
#include "sniffercommit/application/checks/clang_format_check.hpp"
#include "sniffercommit/application/checks/clang_tidy_check.hpp"
#include "sniffercommit/application/checks/compiler_check.hpp"
#include "sniffercommit/application/checks/git_diff_check.hpp"
#include "sniffercommit/application/checks/shell_check.hpp"
#include "sniffercommit/domain/check.hpp"
#include "sniffercommit/domain/error_codes.hpp"
#include "sniffercommit/domain/ports/shell_executor.hpp"
#include "sniffercommit/glob_match.hpp"
#include "sniffercommit/spinner.hpp"
#include "sniffercommit/util.hpp"

namespace sniffercommit::application {

namespace {

using domain::ExitCode;

// Column width for dotted alignment in check result output
constexpr size_t k_result_col = 68;

// Maps a config check to its concrete implementation by command basename.
// Heuristic-based dispatch: clang-format/clang-tidy/compilers/cmake/git get
// specialized behavior; everything else is a custom shell command.
// config if the heuristics start misrouting checks.
bool is_compiler(const std::string& basename) {
  static const std::vector<std::string> k_prefixes = {"gcc",     "g++", "clang",
                                                      "clang++", "cc",  "c++"};
  for (const auto& prefix : k_prefixes) {
    if (basename == prefix ||
        (basename.size() > prefix.size() && basename.compare(0, prefix.size(), prefix) == 0 &&
         basename[prefix.size()] == '-')) {
      return true;
    }
  }
  return false;
}

std::unique_ptr<domain::Check> make_check(const domain::config::Check& config) {
  auto basename = std::filesystem::path(config.command).filename().string();
  if (basename == "clang-format") {
    return std::make_unique<checks::ClangFormatCheck>(config);
  }
  if (basename == "clang-tidy") {
    return std::make_unique<checks::ClangTidyCheck>(config);
  }
  if (is_compiler(basename)) {
    return std::make_unique<checks::CompilerCheck>(config);
  }
  if (basename == "cmake") {
    return std::make_unique<checks::BuildCheck>(config);
  }
  if (basename == "git") {
    return std::make_unique<checks::GitDiffCheck>(config);
  }
  return std::make_unique<checks::ShellCheck>(config);
}

// Thread-safe printer for concurrent check output.
// Checks run in parallel via std::async; without a mutex, output from
// different checks would interleave mid-line. The lock_guard on each
// method ensures atomic writes per call.
class SyncPrinter {
 public:
  void print_check_result(const std::string& name, std::string_view result, int exit_code = 0,
                          std::string_view tool_output = {}, bool verbose = false) {
    std::lock_guard lock(mutex_);
    size_t dot_count = (name.length() < k_result_col) ? k_result_col - name.length() : 1;
    std::cout << name << std::string(dot_count, '.') << result << "\n";
    if (!tool_output.empty() && (exit_code != 0 || verbose)) {
      if (exit_code != 0) {
        std::cout << " - hook id: " << name << "\n";
        std::cout << " - exit code: " << exit_code << "\n";
        std::cout << "\n";
      }
      size_t line_start = 0;
      while (line_start < tool_output.size()) {
        size_t line_end = tool_output.find('\n', line_start);
        std::string_view line = (line_end == std::string::npos)
                                    ? tool_output.substr(line_start)
                                    : tool_output.substr(line_start, line_end - line_start);
        if (!line.empty() && line.find_first_not_of(" \t\r") != std::string::npos) {
          std::cout << " " << line << "\n";
        }
        if (line_end == std::string::npos) {
          break;
        }
        line_start = line_end + 1;
      }
      std::cout << "\n";
    }
  }

  void print_file_result(const std::string& file_name, std::string_view status,
                         size_t indent_col = 2) {
    std::lock_guard lock(mutex_);
    std::string label = std::string(indent_col, ' ') + file_name;
    size_t effective_col = k_result_col - indent_col;
    if (effective_col > label.length()) {
      std::cout << label << std::string(effective_col - label.length(), '.') << status << "\n";
    } else {
      std::cout << label << " " << status << "\n";
    }
  }

  void print_verbose(std::string_view msg) {
    std::lock_guard lock(mutex_);
    std::cout << msg;
  }

  void print_error(std::string_view msg) {
    std::lock_guard lock(mutex_);
    std::cerr << msg;
  }

 private:
  std::mutex mutex_;
};

// Result of running a single check against a set of matched files.
struct CheckResult {
  std::string check_name_{};
  int exit_code_{0};
  std::string output_{};
  bool verbose_{false};
};

// Runs a single check against all matched files through the generic
// Check abstraction. Each check's execute() decides batch vs per-file
// invocation and handles tool presence + output collection.
// than the dedicated MISSING_DEPENDENCY code; restore that code if anything
// distinguishes the two.
CheckResult run_check_for_files(std::unique_ptr<domain::Check> check,
                                const std::vector<std::string>& matched_files,
                                const RunOptions& opts, SyncPrinter& printer,
                                domain::ports::IShellExecutor* shell) {
  auto result = check->execute(matched_files, shell, opts.verbose, opts.dry_run);

  if (result.exit_code == 0) {
    printer.print_check_result(check->name(), "Passed");
  } else {
    printer.print_check_result(check->name(), "Failed", result.exit_code, result.output,
                               opts.verbose);
  }

  return {.check_name_ = check->name(), .exit_code_ = result.exit_code, .output_ = {}};
}

// C/C++ source/header extensions eligible for clang-format.
// Used by the --format mode. Duplicated from ClangFormatCheck because that
// filter is per-file, not per-check, and lives in --format's reporting loop.
// lazy: extension list is hardcoded; could be configurable, but nobody
// has asked for it and this covers the standard set.
bool is_format_eligible(const std::string& file) {
  static const std::vector<std::string> k_format_extensions = {
      ".cpp", ".cc", ".cxx", ".c++", ".hpp", ".h", ".hh", ".hxx", ".inc", ".c"};
  std::filesystem::path file_path(file);
  std::string ext = file_path.extension().string();
  std::ranges::transform(ext, ext.begin(),
                         [](unsigned char chr) { return static_cast<char>(std::tolower(chr)); });
  return std::ranges::any_of(k_format_extensions, [&ext](const auto& e) { return ext == e; });
}

std::vector<std::string> filter_format_files(const std::vector<std::string>& files) {
  std::vector<std::string> result;
  result.reserve(files.size());
  for (const auto& file : files) {
    if (is_format_eligible(file)) {
      result.push_back(file);
    }
  }
  return result;
}

}  // namespace

RunChecksUseCase::RunChecksUseCase(std::unique_ptr<domain::ports::IShellExecutor> shell,
                                   std::unique_ptr<domain::ports::IGitRepository> git_repo,
                                   std::unique_ptr<domain::ports::IFileSystem> fs)
    : shell_(std::move(shell)), git_repo_(std::move(git_repo)), fs_(std::move(fs)) {}

// Collects the list of files to check based on the run mode:
//   STAGED  - git staged files (default for pre-commit hook)
//   ALL_REPO - all tracked files in the repo
//   EXPLICIT - user-specified file list
//
// After collection, files are:
// 1. Filtered against exclude_paths from config
// 2. Sorted and deduplicated
std::vector<std::string> RunChecksUseCase::collect_files(
    const std::filesystem::path& repo_root, const RunOptions& opts,
    const std::vector<std::string>& exclude_paths) {
  std::vector<std::string> files;

  // Exclusion logic supports three patterns:
  //   Exact match:    "build/" matches "build/"
  //   Extension glob: "*.o"    matches "foo.o"
  //   Prefix match:   "build/" matches "build/debug/foo.cpp"
  auto is_excluded = [&exclude_paths](const std::string& file) -> bool {
    for (const auto& excl : exclude_paths) {
      if (file == excl) return true;
      if (excl.starts_with("*.") && file.ends_with(excl.substr(1))) return true;
      std::string norm_e = excl;
      if (!norm_e.empty() && norm_e.back() != '/') norm_e += '/';
      if (file.starts_with(norm_e)) return true;
    }
    return false;
  };

  if (opts.source == FileSource::STAGED) {
    files = git_repo_->list_staged_files(repo_root);
  } else if (opts.source == FileSource::ALL_REPO) {
    files = git_repo_->list_all_files(repo_root);
  } else {
    for (const auto& file_name : opts.explicit_files) {
      std::string rel = std::filesystem::relative(std::filesystem::absolute(file_name), repo_root)
                            .generic_string();
      files.push_back(rel);
    }
  }

  std::erase_if(files, [&](const std::string& f) { return is_excluded(f); });

  std::ranges::sort(files);
  auto [first, last] = std::ranges::unique(files);
  files.erase(first, last);

  return files;
}

// Main entry point for running checks.
// Determines the repo root, collects files, then dispatches to either
// execute_format (for --format mode) or execute_checks (for normal checks).
int RunChecksUseCase::execute(const domain::config::ProjectConfig& cfg, const RunOptions& opts) {
  std::filesystem::path repo_root;
  try {
    repo_root = git_repo_->find_repo_root(fs_->current_path());
  } catch (const std::exception& e) {
    std::cerr << "[ERROR] " << e.what() << "\n";
    return static_cast<int>(ExitCode::NOT_A_GIT_REPO);
  }

  auto files = collect_files(repo_root, opts, cfg.exclude_paths);

  if (opts.mode == RunMode::FORMAT) {
    return execute_format(repo_root, files, opts);
  }

  return execute_checks(repo_root, cfg, files, opts);
}

// Runs all configured checks against the collected files.
//
// Flow:
// 1. Match files to checks using glob patterns
// 2. Validate tool configs exist (clang-tidy, clang-format)
// 3. Run checks either sequentially or in parallel via std::async
//
// The parallel path uses std::future + std::async. Output is serialized
// through SyncPrinter's mutex to prevent interleaving.
int RunChecksUseCase::execute_checks(const std::filesystem::path& repo_root,
                                     const domain::config::ProjectConfig& cfg,
                                     const std::vector<std::string>& files,
                                     const RunOptions& opts) {
  SyncPrinter printer;

  if (opts.dry_run) {
    printer.print_verbose(fmt::format("[DRY-RUN] Would check {} file(s):\n", files.size()));
    for (const auto& file_name : files) {
      printer.print_verbose(fmt::format(" {}\n", file_name));
    }
    return static_cast<int>(ExitCode::SUCCESS);
  }

  // A work item pairs a concrete check with its matched files.
  // Config errors are pre-validated before any execution begins.
  struct WorkItem {
    std::unique_ptr<domain::Check> check;
    std::vector<std::string> matched_files;
    std::string config_error;
  };

  std::vector<WorkItem> work_items;
  work_items.reserve(cfg.checks.size());

  for (const auto& check : cfg.checks) {
    if (!check.enabled) {
      if (opts.verbose) {
        printer.print_verbose(fmt::format("[sniffercommit] [SKIP] {} (disabled)\n", check.name));
      }
      continue;
    }

    std::vector<std::string> matched;
    for (const auto& file_name : files) {
      if (util::matches_any_pattern(file_name, check.patterns)) {
        matched.push_back(file_name);
      }
    }

    if (matched.empty()) {
      if (opts.verbose) {
        printer.print_verbose(fmt::format("[sniffercommit] [SKIP] {}\n", check.name));
      }
      continue;
    }

    auto impl = make_check(check);
    std::string config_err = impl->validate(repo_root);
    work_items.push_back({.check = std::move(impl),
                          .matched_files = std::move(matched),
                          .config_error = std::move(config_err)});
  }

  if (work_items.empty()) {
    return static_cast<int>(ExitCode::SUCCESS);
  }

  bool has_config_errors = false;
  for (const auto& item : work_items) {
    if (!item.config_error.empty()) {
      printer.print_error(fmt::format("[ERROR] {}\n", item.config_error));
      has_config_errors = true;
    }
  }
  if (has_config_errors) {
    return static_cast<int>(ExitCode::CONFIG_ERROR);
  }

  Spinner spinner("Running checks");
  if (!opts.verbose) {
    // spinner started automatically in constructor
  }

  util::CwdGuard cwd_guard(repo_root);

  // Single check or parallel disabled → run sequentially.
  // Sequential is simpler and avoids thread overhead for trivial workloads.
  if (!cfg.parallel || work_items.size() == 1) {
    int exit_code = static_cast<int>(ExitCode::SUCCESS);
    for (auto& item : work_items) {
      auto result = run_check_for_files(std::move(item.check), item.matched_files, opts, printer,
                                        shell_.get());
      if (result.exit_code_ != 0) {
        exit_code = result.exit_code_;
      }
    }

    if (exit_code != 0) {
      printer.print_error("One or more checks failed.\n");
    }
    return exit_code;
  }

  // Parallel execution: each check runs in its own std::async task.
  // shell_ is thread-safe (popen/fork are per-process), so concurrent
  // access is safe. SyncPrinter handles output serialization.
  std::vector<std::future<CheckResult>> futures;
  futures.reserve(work_items.size());

  for (auto& item : work_items) {
    futures.push_back(
        std::async(std::launch::async, [item = std::move(item), &opts, &printer, this]() mutable {
          return run_check_for_files(std::move(item.check), item.matched_files, opts, printer,
                                     shell_.get());
        }));
  }

  int exit_code = static_cast<int>(ExitCode::SUCCESS);
  for (auto& future : futures) {
    auto result = future.get();
    if (result.exit_code_ != 0) {
      exit_code = result.exit_code_;
    }
  }

  if (exit_code != 0) {
    printer.print_error("One or more checks failed.\n");
  }

  return exit_code;
}

// Runs clang-format in-place on eligible C/C++ files.
// After formatting, uses `git diff --quiet` to detect which files actually
// changed (vs already being clean). Reports formatted vs clean counts.
int RunChecksUseCase::execute_format(const std::filesystem::path& repo_root,
                                     const std::vector<std::string>& files,
                                     const RunOptions& opts) {
  SyncPrinter printer;

  if (!shell_->command_exists("clang-format")) {
    printer.print_error(
        "[ERROR] 'clang-format' not found in PATH. Install it or check your configuration.\n");
    return static_cast<int>(ExitCode::MISSING_DEPENDENCY);
  }

  auto orig_cwd = std::filesystem::current_path();
  util::CwdGuard cwd_guard(repo_root);

  // Check for .clang-format or _clang-format (LLVM convention).
  // Without a config, clang-format uses default style which is rarely wanted.
  bool has_config =
      std::filesystem::exists(".clang-format") || std::filesystem::exists("_clang-format");
  if (!has_config) {
    printer.print_error("[ERROR] No .clang-format config found. Run 'sniffercommit init' first.\n");
    std::filesystem::current_path(orig_cwd);
    return static_cast<int>(ExitCode::CONFIG_ERROR);
  }

  auto format_files = filter_format_files(files);
  if (format_files.empty()) {
    printer.print_verbose("[sniffercommit] [INFO] No format-eligible files found.\n");
    std::filesystem::current_path(orig_cwd);
    return static_cast<int>(ExitCode::SUCCESS);
  }

  if (opts.dry_run) {
    printer.print_verbose(fmt::format("[DRY-RUN] Would format {} file(s):\n", format_files.size()));
    for (const auto& file : format_files) {
      printer.print_verbose(fmt::format(" {}\n", file));
    }
    std::filesystem::current_path(orig_cwd);
    return static_cast<int>(ExitCode::SUCCESS);
  }

  if (opts.verbose) {
    printer.print_verbose(
        fmt::format("[sniffercommit] [INFO] Formatting {} file(s)\n", format_files.size()));
  }

  Spinner spinner("Formatting files...");

  int exit_code = 0;
  int formatted_count = 0;
  int clean_count = 0;

  for (const auto& file : format_files) {
    std::string cmd = "clang-format -i " + util::shell_escape(file);
    if (opts.verbose) {
      printer.print_verbose(" $ " + cmd + "\n");
    }

    auto fmt_result = shell_->exec_captured(cmd);

    if (fmt_result.exit_code_ != 0) {
      exit_code = 1;
      printer.print_file_result(file, "Failed");
      continue;
    }

    // git diff --quiet returns 0 if no changes, non-zero if file was modified.
    // This tells us whether clang-format actually changed anything.
    auto diff_result = shell_->exec_captured("git diff --quiet " + util::shell_escape(file));
    if (diff_result.exit_code_ != 0) {
      ++formatted_count;
      printer.print_file_result(file, "Formatted");
    } else {
      ++clean_count;
      if (opts.verbose) {
        printer.print_file_result(file, "Clean");
      }
    }
  }

  std::filesystem::current_path(orig_cwd);

  if (exit_code == 0) {
    if (formatted_count > 0) {
      printer.print_verbose(
          fmt::format("[sniffercommit] [INFO] Formatted {} file(s), {} already clean.\n",
                      formatted_count, clean_count));
      printer.print_verbose("[sniffercommit] [INFO] Stage changes with: git add -u\n");
    }
  } else {
    printer.print_error("[sniffercommit] [ERROR] Formatting failed on some files.\n");
  }

  return exit_code;
}

}  // namespace sniffercommit::application
