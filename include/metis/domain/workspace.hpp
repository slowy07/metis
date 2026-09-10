#ifndef METIS_DOMAIN_WORKSPACE_HPP
#define METIS_DOMAIN_WORKSPACE_HPP

#include <filesystem>
#include <string>
#include <vector>
namespace metis::domain::workspace {
struct WorkspaceMember {
  std::filesystem::path path;
  std::string name;
};

[[nodiscard]] std::vector<std::filesystem::path> resolve_members(
    const std::vector<std::string>& member_globs, const std::vector<std::string>& exclude_globs,
    const std::filesystem::path& workspace_root);
[[nodiscard]] bool is_workspace_member(const std::filesystem::path& candidate,
                                       const std::vector<std::filesystem::path>& members);
}  // namespace metis::domain::workspace

#endif  // !METIS_DOMAIN_WORKSPACE_HPP
