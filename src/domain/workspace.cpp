#include "metis/domain/workspace.hpp"

#include <glob.h>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <vector>

namespace metis::domain::workspace {
std::vector<std::filesystem::path> resolve_members(const std::vector<std::string>& member_globs,
                                                   const std::vector<std::string>& exclude_globs,
                                                   const std::filesystem::path& workspace_root) {
  std::vector<std::filesystem::path> members;

  for (const auto& pattern : member_globs) {
    auto abs_pattern = (workspace_root / pattern).string();
    glob_t glob_result{};

    if (glob(abs_pattern.c_str(), GLOB_ONLYDIR, nullptr, &glob_result) == 0) {
      for (size_t i = 0; i < glob_result.gl_pathc; ++i) {
        std::filesystem::path candidate(glob_result.gl_pathv[i]);

        if (std::filesystem::exists(candidate / ".metis.toml")) {
          members.push_back(candidate);
        }
      }
    }

    globfree(&glob_result);
  }

  if (!exclude_globs.empty()) {
    members.erase(
        std::remove_if(members.begin(), members.end(),
                       [&](const auto& mod_path) {
                         for (const auto& ex : exclude_globs) {
                           auto ex_abs = (workspace_root / ex).string();
                           glob_t ex_glob{};

                           if (glob(ex_abs.c_str(), GLOB_ONLYDIR, nullptr, &ex_glob) == 0) {
                             for (size_t i = 0; i < ex_glob.gl_pathc; ++i) {
                               if (mod_path == std::filesystem::path(ex_glob.gl_pathv[i])) {
                                 globfree(&ex_glob);
                                 return true;
                               }
                             }
                           }
                           globfree(&ex_glob);
                         }
                         return false;
                       }),
        members.end());
  }

  return members;
}

bool is_workspace_member(const std::filesystem::path& candidate,
                         const std::vector<std::filesystem::path>& members) {
  auto abs_candidate = std::filesystem::absolute(candidate);
  return std::ranges::any_of(members, [&](const auto& mod_path) {
    return std::filesystem::absolute(mod_path) == abs_candidate;
  });
}

}  // namespace metis::domain::workspace
