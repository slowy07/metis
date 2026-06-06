#include "sniffercommit/executor.hpp"

#include <fmt/format.h>

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <string>

#include "sniffercommit/error_codes.hpp"
#include "sniffercommit/project_config.hpp"
#include "sniffercommit/util.hpp"

namespace sniffercommit {

namespace {

bool check_command_exists(const project::Check& check) {
  if (!util::command_exists(check.command)) {
    std::cerr << fmt::format(
        "[ERROR] '{}' not found in PATH. Install it or check your configuration.\n", check.command);
    return false;
  }
  return true;
}

bool is_grep_like(const std::string& cmd_basename) {
  return cmd_basename == "grep" || cmd_basename == "egrep" || cmd_basename == "rg";
}

constexpr size_t k_result_col = 68;

static void print_check_result(const std::string& name, std::string_view result, int exit_code = 0,
                               std::string_view tool_output = {}, bool verbose = false) {
  size_t dot_count = (name.length() < k_result_col) ? k_result_col - name.length() : 1;
  std::cout << name << std::string(dot_count, '.') << result << "\n";

  if (!tool_output.empty() && (exit_code != 0 || verbose)) {
    if (exit_code != 0) {
      std::cout << "  - hook id: " << name << "\n";
      std::cout << "  - exit code: " << exit_code << "\n";
      std::cout << "\n";
    }
    size_t line_start = 0;
    while (line_start < tool_output.size()) {
      size_t line_end = tool_output.find('\n', line_start);
      std::string_view line = (line_end == std::string::npos)
                                  ? tool_output.substr(line_start)
                                  : tool_output.substr(line_start, line_end - line_start);
      if (!line.empty() && line.find_first_not_of(" \t\r") != std::string::npos) {
        std::cout << "  " << line << "\n";
      }
      if (line_end == std::string::npos) break;
      line_start = line_end + 1;
    }
    std::cout << "\n";
  }
}

static void print_file_result(const std::string& file_name, std::string_view status,
                              size_t indent_col = 2) {
  std::string label = std::string(indent_col, ' ') + file_name;
  size_t effective_col = k_result_col - indent_col;
  if (effective_col > label.length()) {
    std::cout << label << std::string(effective_col - label.length(), '.') << status << "\n";
  } else {
    std::cout << label << " " << status << "\n";
  }
}

int run_single_check(const std::string& cmd_line, std::string_view check_name,
                     std::string_view target_file, const RunOptions& opts) {
  (void)target_file;

  if (opts.verbose) {
    std::cout << fmt::format(" $ {}\n", cmd_line);
  }

  auto result = util::exec_captured(cmd_line);
  int code = result.exit_code;

  if (code == 0) {
    print_check_result(std::string(check_name), "Passed");
  } else {
    print_check_result(std::string(check_name), "Failed", code, result.output);
  }

  return code;
}

int format_single_file(const std::string& file, const RunOptions& opts) {
  std::string cmd = "clang-format -i " + util::shell_escape(file);
  if (opts.verbose) {
    std::cout << " $ " << cmd << "\n";
  }
  auto result = util::exec_captured(cmd);
  if (result.exit_code != 0) {
    print_file_result(file, "Failed");
    if (!result.output.empty()) {
      std::cout << "  " << result.output << "\n";
    }
    return result.exit_code;
  }
  return 0;
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

  int exit_code = 0;
  int formatted_count = 0;
  int clean_count = 0;

  for (const auto& file : format_files) {
    int code = format_single_file(file, opts);
    if (code != 0) {
      exit_code = 1;
      continue;
    }
    auto diff_result = util::exec_captured("git diff --quiet " + util::shell_escape(file));
    if (diff_result.exit_code != 0) {
      ++formatted_count;
      print_file_result(file, "Formatted");
    } else {
      ++clean_count;
      if (opts.verbose) {
        print_file_result(file, "Clean");
      }
    }
  }

  if (exit_code == 0) {
    if (formatted_count > 0) {
      std::cout << fmt::format("[sniffercommit] [INFO] Formatted {} file(s), {} already clean.\n",
                               formatted_count, clean_count);
      std::cout << "[sniffercommit] [INFO] Stage changes with: git add -u\n";
    }
  } else {
    std::cerr << fmt::format("[sniffercommit] [ERROR] Formatting failed on some files.\n");
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

    std::string cmd = build_check_cmd(check);

    for (const auto& file_name : matched) {
      std::string full = fmt::format("{} {}", cmd, util::shell_escape(file_name));

      int code = run_single_check(full, check.name, file_name, opts);

      auto cmd_basename = std::filesystem::path(check.command).filename().string();
      if (is_grep_like(cmd_basename) && code == 1) {
        code = 0;
      }

      if (code != 0) {
        exit_code = 1;
      }
    }
  }

  if (exit_code != 0) {
    std::cerr << "One or more checks failed.\n";
  }
  return exit_code;
}

}  // namespace sniffercommit
