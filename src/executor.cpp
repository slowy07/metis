#include "../include/sniffercommit/executor.hpp"

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

namespace sniffercommit {

// helper function
std::string shell_escape(const std::string& value) {
  std::string escaped = "\"";

  for (char c : value) {
    if (c == '"') {
      escaped += "\\\"";
    } else {
      escaped += c;
    }
  }

  escaped += "\"";
  return escaped;
}

std::string exec_cmd(const std::string& cmd) {
  std::array<char, 256> buffer;
  std::string result;
  std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"),
                                                pclose);
  if (!pipe) throw std::runtime_error("popen() failed: " + cmd);
  while (fgets(buffer.data(), buffer.size(), pipe.get()))
    result += buffer.data();
  if (!result.empty() && result.back() == '\n') result.pop_back();
  return result;
}

bool matches_pattern(const std::string& file,
                     const std::vector<std::string>& patterns) {
  if (patterns.empty()) return true;
  for (const auto& p : patterns) {
    if (p == "*") return true;
    if (p.starts_with("*.") && file.ends_with(p.substr(1))) return true;
    if (p.ends_with("/**") && file.starts_with(p.substr(0, p.size() - 3) + "/"))
      return true;
    if (file == p || file.starts_with(p)) return true;
  }
  return false;
}

bool is_excluded(const std::string& file,
                 const std::vector<std::string>& excludes) {
  for (const auto& e : excludes) {
    if (file.starts_with(e) || file == e) return true;
    if (e.starts_with("*.") && file.ends_with(e.substr(1))) return true;
  }
  return false;
}

std::vector<std::string> collect_files(
    const std::filesystem::path& root, const RunOptions& opts,
    const std::vector<std::string>& excludes) {
  std::vector<std::string> files;
  std::string cmd;

  if (opts.source == FileSource::STAGED)
    cmd = fmt::format(
        "cd '{}' && git diff --cached --name-only --diff-filter=ACM",
        root.string());
  else if (opts.source == FileSource::ALL_REPO)
    cmd = fmt::format("cd '{}' && git ls-files", root.string());

  if (opts.source != FileSource::EXPLICIT) {
    std::string out = exec_cmd(cmd);
    size_t pos = 0;
    while ((pos = out.find('\n')) != std::string::npos) {
      std::string f = out.substr(0, pos);
      out.erase(0, pos + 1);
      if (!f.empty() && !is_excluded(f, excludes)) files.push_back(f);
    }
    if (!out.empty() && !is_excluded(out, excludes)) files.push_back(out);
  } else {
    for (const auto& f : opts.explicit_files) {
      std::string rel =
          std::filesystem::relative(std::filesystem::absolute(f), root)
              .generic_string();
      if (!is_excluded(rel, excludes)) files.push_back(rel);
    }
  }

  std::sort(files.begin(), files.end());
  files.erase(std::unique(files.begin(), files.end()), files.end());
  return files;
}

int execute_checks(const std::filesystem::path& repo_root, const Config& cfg,
                   const std::vector<std::string>& files,
                   const RunOptions& opts) {
  const auto original_cwd = std::filesystem::current_path();
  std::filesystem::current_path(repo_root);

  int exit_code = 0;

  for (const auto& check : cfg.checks) {
    std::vector<std::string> matched;

    for (const auto& f : files) {
      if (matches_pattern(f, check.patterns)) {
        matched.push_back(f);
      }
    }

    if (matched.empty()) {
      if (opts.verbose) {
        std::cout << fmt::format("[sniffercommit] [SKIP] {}\n", check.name);
      }

      continue;
    }

    if (opts.verbose) {
      std::cout << fmt::format(
          "[sniffercommit] [INFO] Running: {} on {} file(s)\n", check.name,
          matched.size());
    }

    std::string cmd = sniffercommit::shell_escape(check.command);

    for (const auto& a : check.args) {
      cmd += " ";
      cmd += shell_escape(a);
    }

    for (const auto& f : matched) {
      std::string full = fmt::format("{} {}", cmd, shell_escape(f));

      if (opts.verbose) {
        std::cout << fmt::format("  $ {}\n", full);
      }

      int status = std::system(full.c_str());
      int code = WEXITSTATUS(status);

      if (check.command == "grep" && code == 1) {
        code = 0;
      }

      if (code != 0) {
        std::cerr << fmt::format(
            "[sniffercommit] [ERROR] {} failed on {} (exit {})\n", check.name,
            f, code);
        exit_code = 1;
      }
    }
  }

  std::filesystem::current_path(original_cwd);

  if (exit_code == 0)
    std::cout << "[sniffercommit] [INFO] All checks passed.\n";
  else
    std::cerr << "[sniffercommit] [ERROR] One or more checks failed.\n";
  return exit_code;
}

}  // namespace sniffercommit
