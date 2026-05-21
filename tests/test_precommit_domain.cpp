#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include "../include/sniffercommit/precommit_domain.hpp"
#include "../include/sniffercommit/project_config.hpp"

using namespace sniffercommit;
using namespace sniffercommit::precommit;

class PrecommitInstallTest : public ::testing::Test {
 protected:
  void SetUp() override {
    test_root = std::filesystem::temp_directory_path() / "sniffercommit_test";
    git_hooks_dir = test_root / ".git" / "hooks";
    std::filesystem::create_directories(git_hooks_dir);
  }

  void TearDown() override { std::filesystem::remove_all(test_root); }

  std::filesystem::path test_root;
  std::filesystem::path git_hooks_dir;
};

TEST_F(PrecommitInstallTest, InstallCreateHookFile) {
  std::string hook_content = "#!/usr/bin/env bash\nexit 0\n";
  bool result = install(test_root, hook_content);

  EXPECT_TRUE(result);

  auto hook_path = git_hooks_dir / "pre-commit";
  EXPECT_TRUE(std::filesystem::exists(hook_path));

  std::ifstream file(hook_path);
  std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

  EXPECT_EQ(content, hook_content);
}

TEST_F(PrecommitInstallTest, InstallOverwriteExistHook) {
  std::string initial_content = "#!/usr/bin/env bash\nexit 0\n";
  bool result = install(test_root, initial_content);

  std::string new_content = "#!/usr/bin/env bash\nexit 1\n";
  result = install(test_root, new_content);

  EXPECT_TRUE(result);

  auto hook_path = git_hooks_dir / "pre-commit";
  std::ifstream file(hook_path);
  std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

  EXPECT_EQ(content, new_content);
}

TEST_F(PrecommitInstallTest, IsInstalledDetectExistHook) {
  EXPECT_FALSE(is_installed(test_root));
  
  auto hook_path = git_hooks_dir / "pre-commit";
  std::ofstream(hook_path) << "#!/usr/bin/env bash\nexit 0\n";
  
  EXPECT_TRUE(is_installed(test_root));
}
