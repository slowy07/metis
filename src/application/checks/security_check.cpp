#include "metis/application/checks/security_check.hpp"

#include <fmt/format.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <regex>
#include <string>
#include <string_view>
#include <vector>

#include "metis/domain/ports/shell_executor.hpp"

namespace metis::application::checks {
namespace {
const std::array<std::string_view, 10> k_source_extensions = {
    ".cpp", ".cc", ".cxx", ".c++", ".c", ".hpp", ".h", ".hh", ".hxx", ".inc"};

bool is_source_file(const std::filesystem::path& path) {
  std::string ext = path.extension().string();
  std::ranges::transform(ext, ext.begin(),
                         [](unsigned char chr) { return static_cast<char>(std::tolower(chr)); });

  return std::ranges::any_of(k_source_extensions,
                             [&ext](const auto& entry) { return ext == entry; });
}
}  // namespace

SecurityCheck::SecurityCheck(const domain::config::Check& config)
  : domain::Check(config.name, config.description, config.enabled, config.patterns, config.command,
                  config.args, config.timeout, config.severity) {
  init_patterns();
}

void SecurityCheck::init_patterns() {
  patterns_.push_back(
      {.regex = std::regex(R"(\b(strcpy|strcat|sprintf|gets)\s*\()"),
       .category = "dangerous-function",
       .description = "Use of dangerous C function (strcpy, strcat, sprintf, gets)"});

  patterns_.push_back({.regex = std::regex(R"(\b(memcpy|memmove|memset)\s*\()"),
                       .category = "unsafe-memory",
                       .description = "Unsafe memory operation — verify size argument"});
  patterns_.push_back(
      {.regex =
           std::regex(R"((password|passwd|secret|token|api_key|apikey)\s*=\s*[\"'][^\"']+[\"'])",
                      std::regex::icase),
       .category = "hardcoded-secret",
       .description = "Possible hardcoded secret"});

  patterns_.push_back(
      {.regex = std::regex(R"(\b(system|popen|execve|execvp|execl|execlp|execv)\s*\()"),
       .category = "suspicious-syscall",
       .description = "Suspicious system call — validate input sanitization"});
}

bool SecurityCheck::is_comment_line(std::string_view line) {
  auto trimmed = line;
  auto pos = trimmed.find_first_not_of(" \t");

  if (pos != std::string_view::npos) {
    trimmed = trimmed.substr(pos);
  }

  return trimmed.starts_with("//") || trimmed.starts_with("/*") || trimmed.starts_with("*");
}

std::string SecurityCheck::scan_file(const std::filesystem::path& file_path) const {
  std::ifstream file(file_path);

  if (!file.is_open()) {
    return fmt::format("   [ERROR] Cannot open: {}\n", file_path.string());
  }

  std::string findings;
  std::string line;
  int line_number = 0;

  while (std::getline(file, line)) {
    ++line_number;
    if (is_comment_line(line)) {
      continue;
    }

    for (const auto& pattern : patterns_) {
      if (std::regex_search(line, pattern.regex)) {
        findings += fmt::format("  {}:{}: [{}] {}\n  | {}\n", file_path.string(), line_number,
                                pattern.category, pattern.description, line);
      }
    }
  }

  return findings;
}

domain::CheckResult SecurityCheck::execute(const std::vector<std::string>& files,
                                           domain::ports::IShellExecutor* /*shell*/, bool verbose,
                                           bool dry_run) {
  if (dry_run) {
    return {.exit_code = 0, .output = {}};
  }

  std::string output;
  int finding_count = 0;

  for (const auto& file : files) {
    std::filesystem::path file_path(file);
    if (!is_source_file(file_path)) {
      continue;
    }

    if (verbose) {
      output += fmt::format("[metis] [SECURITY] Scanning {}\n", file);
    }

    std::string findings = scan_file(file_path);
    if (!findings.empty()) {
      output += findings;
      ++finding_count;
    }
  }

  if (finding_count > 0) {
    output =
        fmt::format("[metis] [SECURITY] {} security issue(s) found:\n", finding_count) + output;

    return {.exit_code = 1, .output = output};
  }

  if (verbose) {
    output += "[metis] [SECURITY] No issues found\n";
  }

  return {.exit_code = 0, .output = output};
}

}  // namespace metis::application::checks
