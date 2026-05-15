#include "../include/sniffercommit/installer.hpp"

#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace sniffercommit {

std::filesystem::path find_git_root() {
  auto dir = std::filesystem::current_path();
  while (true) {
    if (std::filesystem::exists(dir / ".git")) return dir;
    auto parent = dir.parent_path();
    if (parent == dir) break;
    dir = parent;
  }
  throw std::runtime_error("Not inside a Git repository");
}

bool install_local_hook(const std::filesystem::path& repo_root,
                        const std::string& content) {
  auto hooks_dir = repo_root / ".git" / "hooks";
  std::filesystem::create_directories(hooks_dir);

  auto hook_path = hooks_dir / "pre-commit";
  std::ofstream out(hook_path, std::ios::trunc);
  if (!out) return false;
  out << content;
  out.close();

#ifndef _WIN32
  std::filesystem::permissions(hook_path,
                               std::filesystem::perms::owner_all |
                                   std::filesystem::perms::group_read |
                                   std::filesystem::perms::group_exec |
                                   std::filesystem::perms::others_read |
                                   std::filesystem::perms::others_exec,
                               std::filesystem::perm_options::replace);
#endif
  return true;
}

bool write_github_actions(const std::filesystem::path& repo_root,
                          const std::string& content) {
  auto gh_dir = repo_root / ".github" / "workflows";
  std::filesystem::create_directories(gh_dir);

  auto yml_path = gh_dir / "sniffercommit.yml";
  std::ofstream out(yml_path, std::ios::trunc);
  if (!out) return false;
  out << content;
  out.close();
  return true;
}

}  // namespace sniffercommit
