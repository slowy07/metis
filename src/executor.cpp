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
  void operator()(FILE* fp) const noexcept {
    if (fp) {
      (void)pclose(fp);
    }
  }
};
using PipePtr = std::unique_ptr<FILE, PipeDeleter>;

// helper function
std::string shell_escape(const std::string& value) {
  std::string escaped = "'";
  for (char c : value) {
    if (c == '\'') {
      escaped += "'\''";
    } else {
      escaped += c;
    }
  }
  escaped += "'";
  return escaped;
}

std::string exec_cmd(const std::string& cmd) {
  std::array<char, 4096> buffer;
  std::string result;

  PipePtr pipe(popen(cmd.c_str(), "r"));

  if (!pipe) throw std::runtime_error("popen() failed: " + cmd);
  while (fgets(buffer.data(), buffer.size(), pipe.get())) {
    result += buffer.data();
  }
  if (!result.empty() && result.back() == '\n') result.pop_back();
  return result;
}

bool command_exists(const std::string& cmd) {
  std::string test = "command -v " + shell_escape(cmd) + " >/dev/null 2>&1";
  return std::system(test.c_str()) == 0;
}

bool matches_pattern(const std::string& file, const std::vector<std::string>& patterns) {
  if (patterns.empty()) return true;
  for (const auto& p : patterns) {
    if (p == "*") return true;
    if (p.starts_with("*.") && file.ends_with(p.substr(1))) return true;
    if (p.ends_with("/**") && file.starts_with(p.substr(0, p.size() - 3) + "/")) return true;
    if (p.starts_with("**/")) {
      std::string suffix = p.substr(3);
      if (file.ends_with(suffix)) return true;
    }
    if (file == p || file.starts_with(p)) return true;
  }
  return false;
}

bool is_excluded(const std::string& file, const std::vector<std::string>& excludes) {
  for (const auto& e : excludes) {
    if (file == e) return true;
    if (e.starts_with("*.") && file.ends_with(e.substr(1))) return true;
    std::string norm_e = e;
    if (!norm_e.empty() && norm_e.back() != '/') {
      norm_e += '/';
    }
    if (file.starts_with(norm_e)) return true;
    if (!file.empty() && file + "/" == norm_e) return true;
  }
  return false;
}

std::vector<std::string> collect_files(const std::filesystem::path& root, const RunOptions& opts,
                                       const std::vector<std::string>& excludes) {
  std::vector<std::string> files;
  std::string cmd;

  if (opts.source == FileSource::STAGED)
    cmd = fmt::format("cd {} && git diff --cached --name-only --diff-filter=ACM",
                      shell_escape(root.string()));
  else if (opts.source == FileSource::ALL_REPO)
    cmd = fmt::format("cd {} && git ls-files", shell_escape(root.string()));

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
          std::filesystem::relative(std::filesystem::absolute(f), root).generic_string();
      if (!is_excluded(rel, excludes)) files.push_back(rel);
    }
  }

  std::sort(files.begin(), files.end());
  files.erase(std::unique(files.begin(), files.end()), files.end());
  return files;
}

int execute_checks(const std::filesystem::path& repo_root, const project::ProjectConfig& cfg,
                   const std::vector<std::string>& files, const RunOptions& opts) {
  const auto original_cwd = std::filesystem::current_path();
  std::filesystem::current_path(repo_root);

  if (opts.dry_run) {
    std::cout << "[DRY-RUN] Would check " << files.size() << " file(s):\n";
    for (const auto& f : files) {
      std::cout << "  " << f << "\n";
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
      std::cout << fmt::format("[sniffercommit] [INFO] Running: {} on {} file(s)\n", check.name,
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
        std::cout << fmt::format(" $ {}\n", full);
      }

      int status = std::system(full.c_str());
      int code = 1;

      if (WIFEXITED(status)) {
        code = WEXITSTATUS(status);
      } else if (WIFSIGNALED(status)) {
        int sig = WTERMSIG(status);
        std::cerr << fmt::format("[ERROR] {} killed by signal {} on {}\n", check.name, sig, f);
        code = 128 + sig;
      }

      auto cmd_basename = std::filesystem::path(check.command).filename().string();
      if ((cmd_basename == "grep" || cmd_basename == "egrep" || cmd_basename == "rg") &&
          code == 1) {
        code = 0;
      }

      if (code != 0) {
        std::cerr << fmt::format("[sniffercommit] [ERROR] {} failed on {} (exit {})\n", check.name,
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
