#include "sniffercommit/application/run_checks_use_case.hpp"

#include <fmt/format.h>

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <exception>
#include <filesystem>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "sniffercommit/domain/error_codes.hpp"
#include "sniffercommit/domain/ports/shell_executor.hpp"
#include "sniffercommit/glob_match.hpp"
#include "sniffercommit/spinner.hpp"
#include "sniffercommit/util.hpp"

namespace sniffercommit::application {

namespace {

using domain::ExitCode;
constexpr size_t k_result_col = 68;

// ponytail: replaced IExitCodeInterpreter interface + 3 classes with one function.
// grep/rg invert 0↔1; everything else passes through.
int interpret_exit_code(int raw, std::string_view cmd) {
  auto basename = std::filesystem::path(cmd).filename().string();
  if ((basename == "grep" || basename == "egrep" || basename == "rg") && (raw == 0 || raw == 1)) {
    return raw == 0 ? 1 : 0;
  }
  return raw;
}

bool is_interpreter_failure(int code) { return code != 0; }

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

struct ConfigRequirement {
  std::string tool_name_;
  std::string config_arg_;
  std::string default_file_;
};

std::optional<std::string> extract_config_path(std::string_view arg, std::string_view prefix) {
  if (arg.starts_with(prefix)) {
    return std::string(arg.substr(prefix.length()));
  }
  return std::nullopt;
}

std::string validate_tool_config(const domain::config::Check& check,
                                 const std::filesystem::path& repo_root) {
  static const std::vector<ConfigRequirement> k_config_tools = {
      {.tool_name_ = "clang-tidy", .config_arg_ = "--config-file=", .default_file_ = ".clang-tidy"},
      {.tool_name_ = "clang-format",
       .config_arg_ = "-style=file",
       .default_file_ = ".clang-format"},
  };

  for (const auto& req : k_config_tools) {
    if (check.command != req.tool_name_) {
      continue;
    }

    bool has_explicit_config = false;
    for (const auto& arg : check.args) {
      if (auto path = extract_config_path(arg, req.config_arg_)) {
        has_explicit_config = true;
        std::filesystem::path config_path(*path);
        if (config_path.is_relative()) {
          config_path = repo_root / config_path;
        }
        if (!std::filesystem::exists(config_path)) {
          return fmt::format(
              "Config file not found for `{}`: {}\n"
              " Run `sniffercommit init --enable-clang-tidy` to generate it.\n"
              " or ensure the file exists at the expected location",
              check.name, config_path.string());
        }
      }
    }

    if (!has_explicit_config && !req.default_file_.empty()) {
      auto default_path = repo_root / req.default_file_;
      if (!std::filesystem::exists(default_path)) {
        return fmt::format(
            "Default config file not found for `{}`: {}\n"
            " Run `sniffercommit init` to generate it\n"
            " or create {} manually in the repository root",
            check.name, default_path.string(), req.default_file_);
      }
    }
  }

  return "";
}

struct CheckResult {
  std::string check_name_{};
  int exit_code_{0};
  std::string output_{};
  bool verbose_{false};
};

CheckResult run_check_for_files(const domain::config::Check& check,
                                const std::vector<std::string>& matched_files,
                                const RunOptions& opts, SyncPrinter& printer,
                                domain::ports::IShellExecutor* shell) {
  if (!shell->command_exists(check.command)) {
    printer.print_check_result(
        check.name, "Missing", static_cast<int>(ExitCode::MISSING_DEPENDENCY),
        fmt::format("'{}' not found in PATH. Install it or check your configuration.",
                    check.command),
        opts.verbose);
    return {.check_name_ = check.name,
            .exit_code_ = static_cast<int>(ExitCode::MISSING_DEPENDENCY),
            .output_ = {}};
  }

  std::string cmd_base = util::shell_escape(check.command);
  for (const auto& arg : check.args) {
    cmd_base += " ";
    cmd_base += util::shell_escape(arg);
  }

  int overall_exit = 0;
  std::string accumulated_output{};

  static const std::unordered_set<std::string> k_multi_file_tools = {
      "clang-format", "clang-tidy", "grep", "egrep", "rg", "cppcheck",
  };
  bool batch = matched_files.size() > 1 &&
               k_multi_file_tools.count(std::filesystem::path(check.command).filename().string());

  if (batch) {
    std::string full_cmd = cmd_base;

    for (const auto& file : matched_files) {
      full_cmd += " " + util::shell_escape(file);
    }

    if (opts.verbose) {
      printer.print_verbose(fmt::format(" $ {}\n", full_cmd));
    }

    auto result = shell->exec_captured(full_cmd);
    int code = interpret_exit_code(result.exit_code, check.command);

    if (is_interpreter_failure(code)) {
      overall_exit = code;
      accumulated_output = result.output;
    }
  } else {
    for (const auto& file_name : matched_files) {
      std::string full_cmd = fmt::format("{} {}", cmd_base, util::shell_escape(file_name));
      if (opts.verbose) {
        printer.print_verbose(fmt::format(" $ {}\n", full_cmd));
      }
      auto res = shell->exec_captured(full_cmd);
      int code = interpret_exit_code(res.exit_code, check.command);
      if (is_interpreter_failure(code)) {
        overall_exit = code;
        if (!res.output.empty()) {
          accumulated_output += res.output;
          if (!accumulated_output.empty() && accumulated_output.back() != '\n') {
            accumulated_output += '\n';
          }
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

  return {.check_name_ = check.name, .exit_code_ = overall_exit, .output_ = {}};
}

}  // namespace

RunChecksUseCase::RunChecksUseCase(std::unique_ptr<domain::ports::IShellExecutor> shell,
                                   std::unique_ptr<domain::ports::IGitRepository> git_repo,
                                   std::unique_ptr<domain::ports::IFileSystem> fs)
    : shell_(std::move(shell)), git_repo_(std::move(git_repo)), fs_(std::move(fs)) {}

std::vector<std::string> RunChecksUseCase::collect_files(
    const std::filesystem::path& repo_root, const RunOptions& opts,
    const std::vector<std::string>& exclude_paths) {
  std::vector<std::string> files;

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

  struct WorkItem {
    const domain::config::Check* check;
    std::vector<std::string> matched_files;
    std::string config_error;
  };

  std::vector<WorkItem> work_items;
  work_items.reserve(cfg.checks.size());

  for (const auto& check : cfg.checks) {
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

    std::string config_err = validate_tool_config(check, repo_root);
    work_items.push_back({.check = &check,
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

  if (!cfg.parallel || work_items.size() == 1) {
    int exit_code = static_cast<int>(ExitCode::SUCCESS);
    for (const auto& item : work_items) {
      auto result =
          run_check_for_files(*item.check, item.matched_files, opts, printer, shell_.get());
      if (result.exit_code_ != 0) {
        exit_code = result.exit_code_;
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
    futures.push_back(std::async(std::launch::async, [item, &opts, &printer, this]() {
      return run_check_for_files(*item.check, item.matched_files, opts, printer, shell_.get());
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
  std::filesystem::current_path(repo_root);

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

    if (fmt_result.exit_code != 0) {
      exit_code = 1;
      printer.print_file_result(file, "Failed");
      continue;
    }

    auto diff_result = shell_->exec_captured("git diff --quiet " + util::shell_escape(file));
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
