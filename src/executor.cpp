#include "sniffercommit/executor.hpp"

#include <fmt/format.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iostream>

#ifndef _WIN32
#include <sys/wait.h>
#endif

#include "sniffercommit/error_codes.hpp"
#include "sniffercommit/project_config.hpp"
#include "sniffercommit/util.hpp"

namespace sniffercommit {

namespace {

bool check_command_exists(const project::Check& check) {
  if (!util::command_exists(check.command)) {
    std::cerr << fmt::format(
        "[ERROR] '{}' not found in PATH. Install it or check your configuration.\n",
        check.command);
    return false;
  }
  return true;
}

bool is_grep_like(const std::string& cmd_basename) {
  return cmd_basename == "grep" || cmd_basename == "egrep" || cmd_basename == "rg";
}

int run_single_check(const std::string& cmd_line, std::string_view check_name,
                     std::string_view target_file, const RunOptions& opts) {
  if (opts.verbose) {
    std::cout << fmt::format(" $ {}\n", cmd_line);
  }

  int status = std::system(cmd_line.c_str());  // NOLINT(bugprone-command-processor)
  int code = 1;

#ifdef _WIN32
  code = status;
#else
  if (WIFEXITED(status)) {
    code = WEXITSTATUS(status);
  } else if (WIFSIGNALED(status)) {
    int sig = WTERMSIG(status);
    std::cerr << fmt::format("[ERROR] {} killed by signal {} on {}\n", check_name, sig, target_file);
    code = 128 + sig;
  }
#endif

  return code;
}

struct FormatResult {
  int exit_code = 0;
  int formatted_count = 0;
  int skipped_count = 0;
  int error_count = 0;
};

int run_format_batch(const std::vector<std::string>& format_files, size_t start,
                     size_t end, const RunOptions& opts) {
  std::string cmd = "clang-format -i";
  for (size_t j = start; j < end; ++j) {
    cmd += " " + util::shell_escape(format_files.at(j));
  }

  if (opts.verbose) {
    std::cout << "[sniffercommit] [INFO] Batch " << ((start / 20) + 1) << ": "
              << (end - start) << " file(s)\n";
  }

  int status = std::system(cmd.c_str());  // NOLINT(bugprone-command-processor)
  int code = 1;
#ifdef _WIN32
  code = status;
#else
  if (WIFEXITED(status)) {
    code = WEXITSTATUS(status);
  } else if (WIFSIGNALED(status)) {
    code = 128 + WTERMSIG(status);
  }
#endif

  if (code != 0) {
    std::cerr << fmt::format(
        "[sniffercommit] [ERROR] clang-format failed on batch {} (exit {})\n",
        ((start / 20) + 1), code);
    return code;
  }

  return 0;
}

void report_format_results(const std::vector<std::string>& format_files, size_t start,
                           size_t end, const RunOptions& opts, FormatResult& result) {
  for (size_t j = start; j < end; ++j) {
    std::string diff_cmd =
        fmt::format("git diff --quiet {}", util::shell_escape(format_files.at(j)));
    bool was_modified = (std::system(diff_cmd.c_str()) != 0);  // NOLINT
    if (was_modified) {
      ++result.formatted_count;
      if (opts.verbose) {
        std::cout << "[sniffercommit] [FORMAT] " << format_files.at(j) << "\n";
      }
    } else {
      ++result.skipped_count;
      if (opts.verbose) {
        std::cout << "[sniffercommit] [OK] " << format_files.at(j) << " (already formatted)\n";
      }
    }
  }
}

}  // namespace

static bool is_format_eligible(const std::string& file) {
  static const std::vector<std::string> k_format_extensions = {
      ".cpp", ".cc", ".cxx", ".c++", ".hpp", ".h", ".hh", ".hxx", ".inc", ".c"};

  std::filesystem::path p_data(file);
  std::string ext = p_data.extension().string();
  std::ranges::transform(ext, ext.begin(),
                         [](unsigned char chr) { return static_cast<char>(std::tolower(chr)); });

  return std::ranges::any_of(k_format_extensions,
                             [&ext](const auto& e_dat) { return ext == e_dat; });
}

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

  if (!util::command_exists("clang-format")) {
    std::cerr
        << "[ERROR] 'clang-format' not found in PATH. Install it or check your configuration.\n";
    return static_cast<int>(ExitCode::MISSING_DEPENDENCY);
  }

  bool has_config =
      std::filesystem::exists(".clang-format") || std::filesystem::exists("_clang-format");
  if (!has_config) {
    std::cerr << "[ERROR] No .clang-format config found. Run 'sniffercommit init' first.\n";
    return static_cast<int>(ExitCode::CONFIG_ERROR);
  }

  auto format_files = filter_format_files(files);
  if (format_files.empty()) {
    std::cout << "[sniffercommit] [INFO] No format-eligible files found.\n";
    return static_cast<int>(ExitCode::SUCCESS);
  }

  if (opts.dry_run) {
    std::cout << "[DRY-RUN] Would format " << format_files.size() << " file(s):\n";
    for (const auto& file : format_files) {
      std::cout << "  " << file << "\n";
    }
    return static_cast<int>(ExitCode::SUCCESS);
  }

  if (opts.verbose) {
    std::cout << fmt::format("[sniffercommit] [INFO] Formatting {} file(s)\n", format_files.size());
  }

  constexpr size_t k_batch_size = 20;
  FormatResult result;

  for (size_t i = 0; i < format_files.size(); i += k_batch_size) {
    size_t end = std::min(i + k_batch_size, format_files.size());

    int batch_code = run_format_batch(format_files, i, end, opts);
    if (batch_code != 0) {
      result.exit_code = 1;
      result.error_count += static_cast<int>(end - i);
      continue;
    }

    report_format_results(format_files, i, end, opts, result);
  }

  if (result.exit_code == 0) {
    if (result.formatted_count > 0) {
      std::cout << fmt::format("[sniffercommit] [INFO] Formatted {} file(s), {} already clean.\n",
                               result.formatted_count, result.skipped_count);
      std::cout << "[sniffercommit] [INFO] Stage changes with: git add -u\n";
    } else {
      std::cout << "[sniffercommit] [INFO] All files already formatted.\n";
    }
  } else {
    std::cerr << fmt::format("[sniffercommit] [ERROR] Formatting failed on {} file(s).\n",
                             result.error_count);
  }

  return result.exit_code;
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

static std::string build_check_cmd(const project::Check& check) {
  std::string cmd = util::shell_escape(check.command);
  for (const auto& arg : check.args) {
    cmd += " ";
    cmd += util::shell_escape(arg);
  }
  return cmd;
}

int execute_checks(const std::filesystem::path& repo_root, const project::ProjectConfig& cfg,
                   const std::vector<std::string>& files, const RunOptions& opts) {
  util::CwdGuard cwd_guard(repo_root);

  if (opts.dry_run) {
    std::cout << "[DRY-RUN] Would check " << files.size() << " file(s):\n";
    for (const auto& file_name : files) {
      std::cout << "  " << file_name << "\n";
    }
    return static_cast<int>(ExitCode::SUCCESS);
  }

  int exit_code = 0;

  for (const auto& check : cfg.checks) {
    if (!check_command_exists(check)) {
      exit_code = 1;
      continue;
    }

    auto matched = match_check_files(files, check);

    if (matched.empty()) {
      if (opts.verbose) {
        std::cout << fmt::format("[sniffercommit] [SKIP] {}\n", check.name);
      }
      continue;
    }

    if (opts.verbose) {
      std::cout << fmt::format("[sniffercommit] [INFO] Running: {} on {} file(s)\n", check.name,
                               matched.size());
    }

    std::string cmd = build_check_cmd(check);

    for (const auto& file_name : matched) {
      std::string full = fmt::format("{} {}", cmd, util::shell_escape(file_name));

      int code = run_single_check(full, check.name, file_name, opts);

      auto cmd_basename = std::filesystem::path(check.command).filename().string();
      if (is_grep_like(cmd_basename) && code == 1) {
        code = 0;
      }

      if (code != 0) {
        std::cerr << fmt::format("[sniffercommit] [ERROR] {} failed on {} (exit {})\n", check.name,
                                 file_name, code);
        exit_code = 1;
      }
    }
  }

  if (exit_code == 0) {
    std::cout << "[sniffercommit] [INFO] All checks passed.\n";
  } else {
    std::cerr << "[sniffercommit] [ERROR] One or more checks failed.\n";
  }
  return exit_code;
}

}  // namespace sniffercommit
