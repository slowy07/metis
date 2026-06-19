#include "sniffercommit/infrastructure/cli_git_repository.hpp"

#include <fmt/format.h>

#include <algorithm>
#include <filesystem>
#include <ranges>
#include <string>
#include <vector>

#include "sniffercommit/domain/ports/shell_executor.hpp"

namespace sniffercommit::infrastructure {

namespace {

std::vector<std::string> split_lines(const std::string& output) {
  std::vector<std::string> lines;
  std::string remaining = output;
  size_t pos = 0;
  while ((pos = remaining.find('\n')) != std::string::npos) {
    std::string line = remaining.substr(0, pos);
    remaining.erase(0, pos + 1);
    if (!line.empty()) {
      lines.push_back(line);
    }
  }
  if (!remaining.empty()) {
    lines.push_back(remaining);
  }
  return lines;
}

}  // namespace

CliGitRepository::CliGitRepository(std::unique_ptr<domain::ports::IShellExecutor> shell)
    : shell_(std::move(shell)) {}

std::vector<std::string> CliGitRepository::list_staged_files(
    const std::filesystem::path& repo_root) {
  auto orig = std::filesystem::current_path();
  std::filesystem::current_path(repo_root);
  std::string out = shell_->exec("git diff --cached --name-only --diff-filter=ACM");
  std::filesystem::current_path(orig);
  return split_lines(out);
}

std::vector<std::string> CliGitRepository::list_all_files(const std::filesystem::path& repo_root) {
  auto orig = std::filesystem::current_path();
  std::filesystem::current_path(repo_root);
  std::string out = shell_->exec("git ls-files");
  std::filesystem::current_path(orig);
  return split_lines(out);
}

bool CliGitRepository::is_file_modified(const std::filesystem::path& file) {
  auto result = shell_->exec_captured("git diff --quiet " + file.string());
  return result.exit_code != 0;
}

std::filesystem::path CliGitRepository::find_repo_root(const std::filesystem::path& start) {
  try {
    auto orig = std::filesystem::current_path();
    std::filesystem::current_path(start);
    std::string out = shell_->exec("git rev-parse --show-toplevel");
    std::filesystem::current_path(orig);
    if (!out.empty()) {
      return std::filesystem::path(out);
    }
  } catch (const std::exception&) {
    // fallthrough
  }

  auto dir = std::filesystem::current_path();
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
