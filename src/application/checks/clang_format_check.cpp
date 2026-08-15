#include "sniffercommit/application/checks/clang_format_check.hpp"

#include <fmt/format.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>
#include <vector>

#include "sniffercommit/domain/check.hpp"
#include "sniffercommit/domain/ports/shell_executor.hpp"
#include "sniffercommit/util.hpp"

namespace sniffercommit::application::checks {
namespace {
bool is_format_eligible(const std::string& file) {
  static const std::vector<std::string> k_exts = {".cpp", ".cc", ".cxx", ".c++", ".hpp",
                                                  ".h",   ".hh", ".hxx", ".inc", ".c"};
  std::filesystem::path path_data(file);
  std::string ext = path_data.extension().string();
  std::ranges::transform(ext, ext.begin(),
                         [](unsigned char chr) { return static_cast<char>(std::tolower(chr)); });
  return std::ranges::any_of(k_exts, [&ext](const auto& emsg) { return ext == emsg; });
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

ClangFormatCheck::ClangFormatCheck(const domain::config::Check& config)
    : name_(config.name),
      description_(config.description),
      enabled_(config.enabled),
      patterns_(config.patterns),
      args_(config.args),
      timeout_(config.timeout),
      severity_(config.severity) {
  std::erase(args_, "-i");
}

std::string ClangFormatCheck::name() const { return name_; }
std::string ClangFormatCheck::description() const { return description_; }
bool ClangFormatCheck::enabled() const { return enabled_; }
std::vector<std::string> ClangFormatCheck::file_patterns() const { return patterns_; }
int ClangFormatCheck::timeout() const { return timeout_; }
std::string ClangFormatCheck::severity() const { return severity_; }

std::string ClangFormatCheck::validate(const std::filesystem::path& repo_root) const {
  bool has_config = std::filesystem::exists(repo_root / ".clang-format") ||
                    std::filesystem::exists(repo_root / "_clang-format");
  if (!has_config) {
    return "No .clang-format config found. Run 'sniffercommit init' first.";
  }
  return "";
}

domain::CheckResult ClangFormatCheck::execute(const std::vector<std::string>& files,
                                              domain::ports::IShellExecutor* shell, bool verbose,
                                              bool dry_run) {
  if (!shell->command_exists("clang-format")) {
    return {.exit_code = 1,
            .output = "'clang-format' not found in PATH. Install it or check your configuration."};
  }

  auto format_files = filter_format_files(files);

  if (format_files.empty()) {
    return {.exit_code = 0, .output = {}};
  }

  if (dry_run) {
    std::string out = fmt::format("[DRY-RUN] Would format {} file(s):\n", format_files.size());

    for (const auto& file : format_files) {
      out += fmt::format(" {}\n", file);
    }

    return {.exit_code = 0, .output = out};
  }

  std::string cmd_base = "clang-format -i";
  for (const auto& arg : args_) {
    if (arg != "-i") {
      cmd_base += " ";
      cmd_base += util::shell_escape(arg);
    }
  }

  std::string batch_cmd = cmd_base;
  for (const auto& f_file : format_files) {
    batch_cmd += " " + util::shell_escape(f_file);
  }

  if (verbose) {
    // INFO: builtin info output
    // TODO: create verbose output
  }

  auto fmt_res = shell->exec_captured(batch_cmd);

  if (fmt_res.exit_code_ != 0) {
    return {.exit_code = fmt_res.exit_code_, .output = fmt_res.output_};
  }

  int exit_code = 0;
  int formatted_count = 0;
  int clean_count = 0;
  std::string output;

  if (verbose) {
    output += fmt::format("$ {}\n", batch_cmd);
  }

  for (const auto& file : format_files) {
    auto diff = shell->exec_captured("git diff --quiet " + util::shell_escape(file));

    if (diff.exit_code_ != 0) {
      ++formatted_count;
      output += fmt::format("  {} ... Formatted\n", file);
    } else {
      ++clean_count;

      if (verbose) {
        output += fmt::format("  {} ... Clean\n", file);
      }
    }
  }

  if (formatted_count > 0) {
    output += fmt::format(
        "\nFormatted {} file(s), {} already clean.\n"
        "Stage changes with: git add -u\n",
        formatted_count, clean_count);
  }

  return {.exit_code = exit_code, .output = output};
}

}  // namespace sniffercommit::application::checks
