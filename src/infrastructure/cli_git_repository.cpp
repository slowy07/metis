#include "sniffercommit/infrastructure/cli_git_repository.hpp"

#include <fmt/format.h>

#include <algorithm>
#include <filesystem>
#include <ranges>
#include <string>
#include <vector>

#include "sniffercommit/domain/ports/shell_executor.hpp"
#include "sniffercommit/util.hpp"

namespace sniffercommit::infrastructure {

namespace {

std::vector<std::string> split_lines(const std::string& output) {
  std::vector<std::string> lines;
  if (output.empty()) {
    return lines;
  }

  lines.reserve(static_cast<size_t>(std::count(output.begin(), output.end(), '\n')) + 1);

  size_t start = 0;
  size_t pos = 0;

  while ((pos = output.find('\n', start)) != std::string::npos) {
    if (pos > start) {
      lines.emplace_back(output.substr(start, pos - start));
    }

    start = pos += 1;
  }

  if (start < output.size()) {
    lines.emplace_back(output.substr(start));
  }

  return lines;
}

}  // namespace

CliGitRepository::CliGitRepository(std::unique_ptr<domain::ports::IShellExecutor> shell)
    : shell_(std::move(shell)) {}

std::vector<std::string> CliGitRepository::list_staged_files(
    const std::filesystem::path& repo_root) {
  std::string cmd = "git -C " + util::shell_escape(repo_root.string()) +
                    " diff --cached --name-only --diff-filter=ACM";
  std::string out = shell_->exec(cmd);
  return split_lines(out);
}

std::vector<std::string> CliGitRepository::list_all_files(const std::filesystem::path& repo_root) {
  std::string cmd = "git -C " + util::shell_escape(repo_root.string()) + " ls-files";
  std::string out = shell_->exec(cmd);
  return split_lines(out);
}

std::filesystem::path CliGitRepository::find_repo_root(const std::filesystem::path& start) {
  try {
    std::string cmd = "git -C " + util::shell_escape(start.string()) + " rev-parse --show-toplevel";
    std::string out = shell_->exec(cmd);
    if (!out.empty()) {
      return std::filesystem::path(out);
    }
  } catch (const std::exception&) {
    // fallthrough
  }

  auto dir = std::filesystem::absolute(start);
  while (true) {
    if (std::filesystem::exists(dir / ".git")) {
      return dir;
    }
    auto parent = dir.parent_path();
    if (parent == dir) {
      break;
    }
    dir = parent;
  }

  throw std::runtime_error("Not inside a Git repository or git not in PATH");
}

}  // namespace sniffercommit::infrastructure
