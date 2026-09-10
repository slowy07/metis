#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "metis/domain/workspace.hpp"

namespace {

std::filesystem::path make_tree() {
  auto root = std::filesystem::temp_directory_path() / "metis-ws-test";
  std::filesystem::remove_all(root);
  for (const auto& dir : {"a", "b", "no-toml", "skip"}) {
    auto p = root / dir;
    std::filesystem::create_directories(p);
    if (dir != std::string("no-toml")) {
      std::ofstream(p / ".metis.toml").close();
    }
  }
  return root;
}

TEST(WorkspaceTest, ResolveMembersExpandsGlobsAndExcludes) {
  auto root = make_tree();
  auto members = metis::domain::workspace::resolve_members({"*"}, {"skip"}, root);

  EXPECT_EQ(members.size(), 2u);
  EXPECT_TRUE(metis::domain::workspace::is_workspace_member(root / "a", members));
  EXPECT_TRUE(metis::domain::workspace::is_workspace_member(root / "b", members));
  EXPECT_FALSE(metis::domain::workspace::is_workspace_member(root / "skip", members));
  EXPECT_FALSE(metis::domain::workspace::is_workspace_member(root / "no-toml", members));

  std::filesystem::remove_all(root);
}
}  // namespace