#include "sniffercommit/application/run_checks_use_case.hpp"

#include <fmt/format.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
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
#include <thread>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif

#include "sniffercommit/domain/error_codes.hpp"
#include "sniffercommit/domain/ports/shell_executor.hpp"

namespace sniffercommit::application {

namespace {

using domain::ExitCode;
constexpr size_t k_result_col = 68;

class CwdGuard {
 public:
  explicit CwdGuard(const std::filesystem::path& target) {
    original_ = std::filesystem::current_path();
    std::filesystem::current_path(target);
  }

  ~CwdGuard() {
    try {
      std::filesystem::current_path(original_);
    } catch (const std::exception&) {
      std::cout << "\n";
    }
  }

  CwdGuard(const CwdGuard&) = delete;
  CwdGuard& operator=(const CwdGuard&) = delete;
  CwdGuard(CwdGuard&&) = delete;
  CwdGuard& operator=(CwdGuard&&) = delete;

 private:
  std::filesystem::path original_;
};

class IExitCodeInterpreter {
 public:
  virtual ~IExitCodeInterpreter() = default;
  [[nodiscard]] virtual int interpret(int raw_exit_code) const = 0;
  [[nodiscard]] virtual bool is_failure(int interpreted_code) const = 0;
};

class DefaultInterpreter : public IExitCodeInterpreter {
 public:
  [[nodiscard]] int interpret(int raw_exit_code) const override { return raw_exit_code; }

  [[nodiscard]] bool is_failure(int interpreted_code) const override {
    return interpreted_code != 0;
  }
};

class GrepInterpreter : public IExitCodeInterpreter {
 public:
  [[nodiscard]] int interpret(int raw_exit_code) const override {
    if (raw_exit_code == 0) {
      return 1;
    }

    if (raw_exit_code == 1) {
      return 0;
    }

    return raw_exit_code;
  }

  [[nodiscard]] bool is_failure(int interpreted_code) const override {
    return interpreted_code != 0;
  }
};

class RgInterpreter : public IExitCodeInterpreter {
 public:
  [[nodiscard]] int interpret(int raw_exit_code) const override {
    if (raw_exit_code == 0) {
      return 1;
    }

    if (raw_exit_code == 1) {
      return 0;
    }

    return raw_exit_code;
  }

  [[nodiscard]] bool is_failure(int interpreted_code) const override {
    return interpreted_code != 0;
  }
};

std::unique_ptr<IExitCodeInterpreter> make_interpreter(std::string_view cmd) {
  auto basename = std::filesystem::path(cmd).filename().string();
  if (basename == "grep" || basename == "egrep") {
    return std::make_unique<GrepInterpreter>();
  }

  if (basename == "rg") {
    return std::make_unique<RgInterpreter>();
  }

  return std::make_unique<DefaultInterpreter>();
}

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
  std::filesystem::path p(file);
  std::string ext = p.extension().string();
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

std::string shell_escape(const std::string& value) {
#ifdef _WIN32
  if (value.find(' ') == std::string::npos && value.find('\t') == std::string::npos) {
    return value;
  }
  std::string escaped = "\"";
  for (char chr : value) {
    if (chr == '"') {
      escaped += "\"\"";
    } else {
      escaped += chr;
    }
  }
  escaped += "\"";
  return escaped;
#else
  std::string escaped = "'";
  for (char chr : value) {
    if (chr == '\'') {
      escaped += "'\\''";
    } else {
      escaped += chr;
    }
  }
  escaped += "'";
  return escaped;
#endif
}

struct ConfigRequirement {
  std::string tool_name;
  std::string config_arg;
  std::string default_file;
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
      {.tool_name = "clang-tidy", .config_arg = "--config-file=", .default_file = ".clang-tidy"},
      {.tool_name = "clang-format", .config_arg = "-style=file", .default_file = ".clang-format"},
  };

  for (const auto& req : k_config_tools) {
    if (check.command != req.tool_name) {
      continue;
    }

    bool has_explicit_config = false;
    for (const auto& arg : check.args) {
      if (auto path = extract_config_path(arg, req.config_arg)) {
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

    if (!has_explicit_config && !req.default_file.empty()) {
      auto default_path = repo_root / req.default_file;
      if (!std::filesystem::exists(default_path)) {
        return fmt::format(
            "Default config file not found for `{}`: {}\n"
            " Run `sniffercommit init` to generate it\n"
            " or create {} manually in the repository root",
            check.name, default_path.string(), req.default_file);
      }
    }
  }

  return "";
}

struct CheckResult {
  std::string check_name;
  int exit_code{0};
  std::string output;
  bool verbose{false};
};

class Spinner {
 public:
  explicit Spinner(std::string_view message)
      : message_(message),
        frames_({"⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"}),
        interval_ms_(80) {
    bool expected = false;
    if (!running_.compare_exchange_strong(expected, true)) {
      return;
    }
    thread_ = std::thread(&Spinner::run_loop, this);
  }

  ~Spinner() { stop(); }

  void stop() {
    if (!running_.load()) {
      return;
    }
    stop_requested_.store(true);
    cv_.notify_all();
    if (thread_.joinable()) {
      thread_.join();
    }
    running_.store(false);
    clear_line();
  }

 private:
  void run_loop() {
    size_t frame_idx = 0;
    while (!stop_requested_.load()) {
      std::cout << "\r" << message_ << " " << frames_[frame_idx] << " " << std::flush;
      frame_idx = (frame_idx + 1) % frames_.size();
      std::unique_lock<std::mutex> lock(mutex_);
      cv_.wait_for(lock, interval_ms_, [this] { return stop_requested_.load(); });
    }
  }

  void clear_line() {
    std::cout << "\r" << std::string(message_.size() + frames_[0].size() + 2, ' ') << "\r"
              << std::flush;
  }

  std::string message_;
  std::vector<std::string> frames_;
  std::chrono::milliseconds interval_ms_;
  std::atomic<bool> running_{false};
  std::atomic<bool> stop_requested_{false};
  std::thread thread_;
  std::mutex mutex_;
  std::condition_variable cv_;
};

bool check_command_exists(const std::string& cmd) {
  static std::mutex cache_mutex;
  static std::unordered_map<std::string, bool> cache;

  {
    std::lock_guard<std::mutex> lock(cache_mutex);
    if (auto iter = cache.find(cmd); iter != cache.end()) {
      return iter->second;
    }
  }

#ifdef _WIN32
  std::string test_cmd = "where " + shell_escape(cmd) + " >nul 2>&1";
  bool exists = (std::system(test_cmd.c_str()) == 0);
#else
  bool exists = false;
  if (cmd.find('/') != std::string::npos) {
    exists = (::access(cmd.c_str(), X_OK) == 0);
  } else {
    const char* path_env = std::getenv("PATH");
    if (path_env != nullptr) {
      std::string path_copy = path_env;
      size_t start = 0;
      while (start < path_copy.size()) {
        size_t end = path_copy.find(':', start);
        std::string dir = path_copy.substr(start, end - start);
        if (!dir.empty()) {
          std::string full = dir + '/' + cmd;
          if (::access(full.c_str(), X_OK) == 0) {
            exists = true;
            break;
          }
        }
        if (end == std::string::npos) break;
        start = end + 1;
      }
    }
  }
#endif

  std::lock_guard<std::mutex> lock(cache_mutex);
  cache[cmd] = exists;
  return exists;
}

CheckResult run_check_for_files(const domain::config::Check& check,
                                const std::vector<std::string>& matched_files,
                                const RunOptions& opts, SyncPrinter& printer,
                                domain::ports::IShellExecutor* shell) {
  if (!check_command_exists(check.command)) {
    printer.print_check_result(
        check.name, "Missing", static_cast<int>(ExitCode::MISSING_DEPENDENCY),
        fmt::format("'{}' not found in PATH. Install it or check your configuration.",
                    check.command),
        opts.verbose);
    return {.check_name = check.name,
            .exit_code = static_cast<int>(ExitCode::MISSING_DEPENDENCY),
            .output = {}};
  }

  std::string cmd_base = shell_escape(check.command);
  for (const auto& arg : check.args) {
    cmd_base += " ";
    cmd_base += shell_escape(arg);
  }

  int overall_exit = 0;
  std::string accumulated_output;

  auto interpreter = make_interpreter(check.command);

  for (const auto& file_name : matched_files) {
    std::string full_cmd = fmt::format("{} {}", cmd_base, shell_escape(file_name));

    if (opts.verbose) {
      printer.print_verbose(fmt::format(" $ {}\n", full_cmd));
    }

    auto result = shell->exec_captured(full_cmd);
    int code = interpreter->interpret(result.exit_code);

    if (interpreter->is_failure(code)) {
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

  return {.check_name = check.name, .exit_code = overall_exit, .output = {}};
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
      if (check.patterns.empty() || std::ranges::any_of(check.patterns, [&](const auto& pattern) {
            if (pattern.empty()) return true;
            if (pattern.starts_with("*.") && file_name.ends_with(pattern.substr(1))) return true;
            if (pattern.ends_with("/**") &&
                file_name.starts_with(pattern.substr(0, pattern.size() - 3) + "/"))
              return true;
            if (pattern.starts_with("**/")) {
              if (file_name.ends_with(pattern.substr(3))) return true;
            }
            if (file_name == pattern || file_name.starts_with(pattern)) return true;
            return false;
          })) {
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

  CwdGuard cwd_guard(repo_root);

  if (!cfg.parallel || work_items.size() == 1) {
    int exit_code = static_cast<int>(ExitCode::SUCCESS);
    for (const auto& item : work_items) {
      auto result =
          run_check_for_files(*item.check, item.matched_files, opts, printer, shell_.get());
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
    futures.push_back(std::async(std::launch::async, [&item, &opts, &printer, this]() {
      return run_check_for_files(*item.check, item.matched_files, opts, printer, shell_.get());
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

int RunChecksUseCase::execute_format(const std::filesystem::path& repo_root,
                                     const std::vector<std::string>& files,
                                     const RunOptions& opts) {
  SyncPrinter printer;

  if (!check_command_exists("clang-format")) {
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
    std::string cmd = "clang-format -i " + shell_escape(file);
    if (opts.verbose) {
      printer.print_verbose(" $ " + cmd + "\n");
    }

    auto fmt_result = shell_->exec_captured(cmd);

    if (fmt_result.exit_code != 0) {
      exit_code = 1;
      printer.print_file_result(file, "Failed");
      continue;
    }

    auto diff_result = shell_->exec_captured("git diff --quiet " + shell_escape(file));
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
