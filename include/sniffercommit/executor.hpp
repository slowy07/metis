#ifndef SNIFFERCOMMIT_EXECUTOR_HPP
#define SNIFFERCOMMIT_EXECUTOR_HPP

#include <filesystem>
#include <string>
#include <vector>

#include "config.hpp"
namespace sniffercommit {

enum class FileSource {
  STAGED,    // git diff --cached
  ALL_REPO,  // git ls-files
  EXPLICIT,  // user-providing file list
};

struct RunOptions {
  FileSource source = FileSource::STAGED;
  std::vector<std::string> explicit_files;
  bool verbose = false;
  bool dry_run = false;
};

std::vector<std::string> collect_files(
    const std::filesystem::path& repo_root, const RunOptions& opts,
    const std::vector<std::string>& exclude_paths);

int execute_checks(const std::filesystem::path& repo_root, const Config& cfg,
                   const std::vector<std::string>& files,
                   const RunOptions& opts);

}  // namespace sniffercommit

#endif  // !SNIFFERCOMMIT_EXECUTOR_HPP
