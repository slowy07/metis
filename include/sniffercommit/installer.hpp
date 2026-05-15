#ifndef SNIFFERCOMMIT_INSTALLER_HPP
#define SNIFFERCOMMIT_INSTALLER_HPP

#include <filesystem>
#include <string>

namespace sniffercommit {
std::filesystem::path find_git_root();
bool install_local_hook(const std::filesystem::path &repo_root,
                        const std::string &content);
bool write_github_actions(const std::filesystem::path &repo_root,
                          const std::string &content);
} // namespace sniffercommit

#endif // !SNIFFERCOMMIT_INSTALLER_HPP
