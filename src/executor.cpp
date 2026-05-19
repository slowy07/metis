#include "sniffercommit/executor.hpp"

#include <fmt/format.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

#include "sniffercommit/project_config.hpp"

namespace sniffercommit {

// custom deleter
struct PipeDeleter {
  void operator()(FILE* file_ptr) const noexcept {
    if (file_ptr != nullptr) {
      (void)pclose(file_ptr);
    }
  }
};
using PipePtr = std::unique_ptr<FILE, PipeDeleter>;

// helper function
std::string shell_escape(const std::string& value) {
  std::string escaped = "'";
  for (char chr : value) {
    if (chr == '\'') {
      escaped += "'\''";
    } else {
      escaped += chr;
    }
  }
  escaped += "'";
  return escaped;
}

std::string exec_cmd(const std::string& cmd) {
  std::array<char, 4096> buffer{};
  std::string result;

  PipePtr pipe(popen(cmd.c_str(), "r"));  // NOLINT(bugprone-command-processor)

  if (!pipe) {
    throw std::runtime_error("popen() failed: " + cmd);
  }
  while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
    result += buffer.data();
  }
  if (!result.empty() && result.back() == '\n') {
    result.pop_back();
  }
  return result;
}

bool command_exists(const std::string& cmd) {
  std::string test = "command -v " + shell_escape(cmd) + " >/dev/null 2>&1";
  return std::system(test.c_str()) == 0;  // NOLINT(bugprone-command-processor)
}

bool matches_pattern(const std::string& file, const std::vector<std::string>& patterns) {
  if (patterns.empty()) {
    return true;
  }
  return std::ranges::any_of(patterns, [&file](const auto& pattern) {
    if (pattern == "*") {
      return true;
    }
    if (pattern.starts_with("*.") && file.ends_with(pattern.substr(1))) {
      return true;
    }
    if (pattern.ends_with("/**") && file.starts_with(pattern.substr(0, pattern.size() - 3) + "/")) {
      return true;
    }
    if (pattern.starts_with("**/")) {
      std::string suffix = pattern.substr(3);
      if (file.ends_with(suffix)) {
        return true;
      }
    }
    if (file == pattern || file.starts_with(pattern)) {
      return true;
    }
    return false;
  });
}

bool is_excluded(const std::string& file, const std::vector<std::string>& excludes) {
  for (const auto& excl : excludes) {
    if (file == excl) {
      return true;
    }
    if (excl.starts_with("*.") && file.ends_with(excl.substr(1))) {
      return true;
    }
    std::string norm_e = excl;
    if (!norm_e.empty() && norm_e.back() != '/') {
      norm_e += '/';
    }
    if (file.starts_with(norm_e)) {
      return true;
    }
    if (!file.empty() && file + "/" == norm_e) {
      return true;
    }
  }
  return false;
}

std::vector<std::string> collect_files(const std::filesystem::path& root, const RunOptions& opts,
                                       const std::vector<std::string>& exclude_paths) {
  std::vector<std::string> files;
  std::string cmd;

  if (opts.source == FileSource::STAGED) {
    cmd = fmt::format("cd {} && git diff --cached --name-only --diff-filter=ACM",
                      shell_escape(root.string()));
  } else if (opts.source == FileSource::ALL_REPO) {
    cmd = fmt::format("cd {} && git ls-files", shell_escape(root.string()));
  }

  if (opts.source != FileSource::EXPLICIT) {
    std::string out = exec_cmd(cmd);
    size_t pos = 0;
    while ((pos = out.find('\n')) != std::string::npos) {
      std::string file_name = out.substr(0, pos);
      out.erase(0, pos + 1);
      if (!file_name.empty() && !is_excluded(file_name, exclude_paths)) {
        files.push_back(file_name);
      }
    }
    if (!out.empty() && !is_excluded(out, exclude_paths)) {
      files.push_back(out);
    }
  } else {
    for (const auto& file_name : opts.explicit_files) {
      std::string rel =
          std::filesystem::relative(std::filesystem::absolute(file_name), root).generic_string();
      if (!is_excluded(rel, exclude_paths)) {
        files.push_back(rel);
      }
    }
  }

  std::ranges::sort(files);
  auto [unique_first, unique_last] = std::ranges::unique(files);
  files.erase(unique_first, unique_last);
  return files;
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
int execute_checks(const std::filesystem::path& repo_root, const project::ProjectConfig& cfg,
                   const std::vector<std::string>& files, const RunOptions& opts) {
  const auto original_cwd = std::filesystem::current_path();
  std::filesystem::current_path(repo_root);

  if (opts.dry_run) {
    std::cout << "[DRY-RUN] Would check " << files.size() << " file(s):\n";
    for (const auto& file_name : files) {
      std::cout << "  " << file_name << "\n";
    }
    std::filesystem::current_path(original_cwd);
    return 0;
  }

  int exit_code = 0;

  for (const auto& check : cfg.checks) {
    if (!command_exists(check.command)) {
      std::cerr << fmt::format(
          "[ERROR] '{}' not found in PATH. Install it or check your configuration.\n",
          check.command);
      exit_code = 1;
      continue;
    }

    std::vector<std::string> matched;
    for (const auto& file_name : files) {
      if (matches_pattern(file_name, check.patterns)) {
        matched.push_back(file_name);
      }
    }

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

    std::string cmd = sniffercommit::shell_escape(check.command);
    for (const auto& arg : check.args) {
      cmd += " ";
      cmd += shell_escape(arg);
    }

    for (const auto& file_name : matched) {
      std::string full = fmt::format("{} {}", cmd, shell_escape(file_name));

      if (opts.verbose) {
        std::cout << fmt::format(" $ {}\n", full);
      }

      int status = std::system(full.c_str());  // NOLINT(bugprone-command-processor)
      int code = 1;

      if (WIFEXITED(status)) {
        code = WEXITSTATUS(status);
      } else if (WIFSIGNALED(status)) {
        int sig = WTERMSIG(status);
        std::cerr << fmt::format("[ERROR] {} killed by signal {} on {}\n", check.name, sig,
                                 file_name);
        code = 128 + sig;
      }

      auto cmd_basename = std::filesystem::path(check.command).filename().string();
      if ((cmd_basename == "grep" || cmd_basename == "egrep" || cmd_basename == "rg") &&
          code == 1) {
        code = 0;
      }

      if (code != 0) {
        std::cerr << fmt::format("[sniffercommit] [ERROR] {} failed on {} (exit {})\n", check.name,
                                 file_name, code);
        exit_code = 1;
      }
    }
  }

  std::filesystem::current_path(original_cwd);

  if (exit_code == 0) {
    std::cout << "[sniffercommit] [INFO] All checks passed.\n";
  } else {
    std::cerr << "[sniffercommit] [ERROR] One or more checks failed.\n";
  }
  return exit_code;
}

}  // namespace sniffercommit
