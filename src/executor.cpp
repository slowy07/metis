#include "sniffercommit/executor.hpp"

#include <fmt/format.h>

#include <algorithm>
#include <filesystem>
#include <future>
#include <iostream>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>

#include "sniffercommit/error_codes.hpp"
#include "sniffercommit/project_config.hpp"
#include "sniffercommit/util.hpp"

namespace sniffercommit {

namespace {

bool check_command_exists(const std::string& cmd) {
  static std::mutex cache_mutex;
  static std::unordered_map<std::string, bool> cache;

  {
    std::lock_guard<std::mutex> lock(cache_mutex);
    if (auto iter = cache.find(cmd); iter != cache.end()) {
      return iter->second;
    }
  }

  bool exists = util::command_exists(cmd);

  {
    std::lock_guard<std::mutex> lock(cache_mutex);
    cache[cmd] = exists;
  }

  return exists;
}

bool is_grep_like(const std::string& cmd_basename) {
  return cmd_basename == "grep" || cmd_basename == "egrep" || cmd_basename == "rg";
}

constexpr size_t k_result_col = 68;

class SyncPrinter {
 public:
  void print_check_result(const std::string& name, std::string_view result, int exit_code = 0,
                          std::string_view tool_output = {}, bool verbose = false) {
    std::lock_guard<std::mutex> lock(mutex_);

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
    std::lock_guard<std::mutex> lock(mutex_);

    std::string label = std::string(indent_col, ' ') + file_name;
    size_t effective_col = k_result_col - indent_col;
    if (effective_col > label.length()) {
      std::cout << label << std::string(effective_col - label.length(), '.') << status << "\n";
    } else {
      std::cout << label << " " << status << "\n";
    }
  }

  void print_verbose(std::string_view msg) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::cout << msg;
  }

  void print_error(std::string_view msg) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::cerr << msg;
  }

 private:
  std::mutex mutex_;
};

struct CheckResult {
  std::string check_name;
  int exit_code{0};
  std::string output;
  bool verbose{false};
};

CheckResult run_check_for_files(const project::Check& check,
                                const std::vector<std::string>& matched_files,
                                const RunOptions& opts, SyncPrinter& printer) {
  if (!check_command_exists(check.command)) {
    printer.print_check_result(
        check.name, "Missing", static_cast<int>(ExitCode::MISSING_DEPENDENCY),
        fmt::format("'{}' not found in PATH. Install it or check your configuration.",
                    check.command),
        opts.verbose);
    return {.check_name = check.name, .exit_code = static_cast<int>(ExitCode::MISSING_DEPENDENCY), .output = {}, .verbose = false};
  }

  std::string cmd_base = util::shell_escape(check.command);

  for (const auto& arg : check.args) {
    cmd_base += " ";
    cmd_base += util::shell_escape(arg);
  }

  int overall_exit = 0;
  std::string accumulated_output;

  for (const auto& file_name : matched_files) {
    std::string full_cmd = fmt::format("{} {}", cmd_base, util::shell_escape(file_name));

    if (opts.verbose) {
      printer.print_verbose(fmt::format(" $ {}\n", full_cmd));
    }

    auto result = util::exec_captured(full_cmd);
    int code = result.exit_code;

    auto cmd_basename = std::filesystem::path(check.command).filename().string();
    if (is_grep_like(cmd_basename) && code == 1) {
      code = 0;
    }

    if (code != 0) {
      overall_exit = code;
      if (!result.output.empty()) {
        accumulated_output += result.output;
        if (!accumulated_output.empty() && accumulated_output.back() != '\n') {
          accumulated_output += '\n';
        }
      }
    }
  }

  if (overall_exit == 0) {
    printer.print_check_result(check.name, "Passed");
  } else {
    printer.print_check_result(check.name, "Failed", overall_exit, accumulated_output,
                               opts.verbose);
  }

  return {.check_name = check.name, .exit_code = overall_exit, .output = {}, .verbose = false};
}

bool is_format_eligible(const std::string& file) {
  static const std::vector<std::string> k_format_extensions = {
      ".cpp", ".cc", ".cxx", ".c++", ".hpp", ".h", ".hh", ".hxx", ".inc", ".c"};

  std::filesystem::path p_data(file);
  std::string ext = p_data.extension().string();
  std::ranges::transform(ext, ext.begin(),
                         [](unsigned char chr) { return static_cast<char>(std::tolower(chr)); });

  return std::ranges::any_of(k_format_extensions,
                             [&ext](const auto& e_dat) { return ext == e_dat; });
}

int format_single_file(const std::string& file, const RunOptions& opts, SyncPrinter& printer) {
  std::string cmd = "clang-format -i " + util::shell_escape(file);
  if (opts.verbose) {
    printer.print_verbose(" $ " + cmd + "\n");
  }
  auto result = util::exec_captured(cmd);
  if (result.exit_code != 0) {
    printer.print_file_result(file, "Failed");
    if (!result.output.empty()) {
      printer.print_error(" " + result.output + "\n");
    }
    return result.exit_code;
  }
  return 0;
}

}  // namespace

static std::vector<std::string> filter_format_files(const std::vector<std::string>& files) {
  std::vector<std::string> result;
  result.reserve(files.size());
  for (const auto& file : files) {
    if (is_format_eligible(file)) {
      result.push_back(file);
    }
  }
  return result;
}

std::vector<std::string> collect_files(const std::filesystem::path& root, const RunOptions& opts,
                                       const std::vector<std::string>& exclude_paths) {
  std::vector<std::string> files;

  if (opts.source != FileSource::EXPLICIT) {
    std::string cmd;
    if (opts.source == FileSource::STAGED) {
      cmd = fmt::format("cd {} && git diff --cached --name-only --diff-filter=ACM",
                        util::shell_escape(root.string()));
    } else {
      cmd = fmt::format("cd {} && git ls-files", util::shell_escape(root.string()));
    }

    std::string out = util::exec_cmd(cmd);
    size_t pos = 0;
    while ((pos = out.find('\n')) != std::string::npos) {
      std::string file_name = out.substr(0, pos);
      out.erase(0, pos + 1);
      if (!file_name.empty() && !util::is_excluded(file_name, exclude_paths)) {
        files.push_back(file_name);
      }
    }
    if (!out.empty() && !util::is_excluded(out, exclude_paths)) {
      files.push_back(out);
    }
  } else {
    for (const auto& file_name : opts.explicit_files) {
      std::string rel =
          std::filesystem::relative(std::filesystem::absolute(file_name), root).generic_string();
      if (!util::is_excluded(rel, exclude_paths)) {
        files.push_back(rel);
      }
    }
  }

  std::ranges::sort(files);
  auto [unique_first, unique_last] = std::ranges::unique(files);
  files.erase(unique_first, unique_last);
  return files;
}

int execute_format(const std::filesystem::path& repo_root, const std::vector<std::string>& files,
                   const RunOptions& opts) {
  util::CwdGuard cwd_guard(repo_root);
  SyncPrinter printer;

  if (!util::command_exists("clang-format")) {
    printer.print_error(
        "[ERROR] 'clang-format' not found in PATH. Install it or check your "
        "configuration.\n");
    return static_cast<int>(ExitCode::MISSING_DEPENDENCY);
  }

  bool has_config =
      std::filesystem::exists(".clang-format") || std::filesystem::exists("_clang-format");
  if (!has_config) {
    printer.print_error("[ERROR] No .clang-format config found. Run 'sniffercommit init' first.\n");
    return static_cast<int>(ExitCode::CONFIG_ERROR);
  }

  auto format_files = filter_format_files(files);
  if (format_files.empty()) {
    printer.print_verbose("[sniffercommit] [INFO] No format-eligible files found.\n");
    return static_cast<int>(ExitCode::SUCCESS);
  }

  if (opts.dry_run) {
    printer.print_verbose(fmt::format("[DRY-RUN] Would format {} file(s):\n", format_files.size()));
    for (const auto& file : format_files) {
      printer.print_verbose(fmt::format(" {}\n", file));
    }
    return static_cast<int>(ExitCode::SUCCESS);
  }

  if (opts.verbose) {
    printer.print_verbose(
        fmt::format("[sniffercommit] [INFO] Formatting {} file(s)\n", format_files.size()));
  }

  int exit_code = 0;
  int formatted_count = 0;
  int clean_count = 0;

  for (const auto& file : format_files) {
    int code = format_single_file(file, opts, printer);
    if (code != 0) {
      exit_code = 1;
      continue;
    }
    auto diff_result = util::exec_captured("git diff --quiet " + util::shell_escape(file));
    if (diff_result.exit_code != 0) {
      ++formatted_count;
      printer.print_file_result(file, "Formatted");
    } else {
      ++clean_count;
      if (opts.verbose) {
        printer.print_file_result(file, "Clean");
      }
    }
  }

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

static std::vector<std::string> match_check_files(const std::vector<std::string>& files,
                                                  const project::Check& check) {
  std::vector<std::string> matched;
  for (const auto& file_name : files) {
    if (util::matches_pattern(file_name, check.patterns)) {
      matched.push_back(file_name);
    }
  }
  return matched;
}

int execute_checks(const std::filesystem::path& repo_root, const project::ProjectConfig& cfg,
                   const std::vector<std::string>& files, const RunOptions& opts) {
  util::CwdGuard cwd_guard(repo_root);
  SyncPrinter printer;

  if (opts.dry_run) {
    printer.print_verbose(fmt::format("[DRY-RUN] Would check {} file(s):\n", files.size()));
    for (const auto& file_name : files) {
      printer.print_verbose(fmt::format(" {}\n", file_name));
    }
    return static_cast<int>(ExitCode::SUCCESS);
  }

  struct WorkItem {
    const project::Check* check;
    std::vector<std::string> matched_files;
  };

  std::vector<WorkItem> work_items;
  work_items.reserve(cfg.checks.size());

  for (const auto& check : cfg.checks) {
    auto matched = match_check_files(files, check);
    if (matched.empty()) {
      if (opts.verbose) {
        printer.print_verbose(fmt::format("[sniffercommit] [SKIP] {}\n", check.name));
      }
      continue;
    }
    work_items.push_back({.check = &check, .matched_files = std::move(matched)});
  }

  if (work_items.empty()) {
    return static_cast<int>(ExitCode::SUCCESS);
  }

  if (!cfg.parallel || work_items.size() == 1) {
    int exit_code = static_cast<int>(ExitCode::SUCCESS);
    for (const auto& item : work_items) {
      auto result = run_check_for_files(*item.check, item.matched_files, opts, printer);
      if (result.exit_code != 0) {
        exit_code = result.exit_code;
      }
    }
    if (exit_code != 0) {
      printer.print_error("One or more checks failed.\n");
    }
    return exit_code;
  }

  std::vector<std::future<CheckResult>> futures;
  futures.reserve(work_items.size());

  for (const auto& item : work_items) {
    futures.push_back(std::async(std::launch::async, [&]() {
      return run_check_for_files(*item.check, item.matched_files, opts, printer);
    }));
  }

  int exit_code = static_cast<int>(ExitCode::SUCCESS);
  for (auto& future : futures) {
    auto result = future.get();
    if (result.exit_code != 0) {
      exit_code = result.exit_code;
    }
  }

  if (exit_code != 0) {
    printer.print_error("One or more checks failed.\n");
  }

  return exit_code;
}

}  // namespace sniffercommit
