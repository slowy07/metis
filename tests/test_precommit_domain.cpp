#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <memory>

#include "sniffercommit/application/install_use_case.hpp"
#include "sniffercommit/domain/config.hpp"
#include "sniffercommit/infrastructure/cli_git_repository.hpp"
#include "sniffercommit/infrastructure/os_file_system.hpp"
#include "sniffercommit/infrastructure/process_shell_executor.hpp"

using namespace sniffercommit;

class InstallTest : public ::testing::Test {
 protected:
  void SetUp() override {
    test_root = std::filesystem::temp_directory_path() / "sniffercommit_install_test";
    git_dir = test_root / ".git";
    hooks_dir = git_dir / "hooks";
    std::filesystem::create_directories(hooks_dir);
  }

  void TearDown() override { std::filesystem::remove_all(test_root); }

  std::filesystem::path test_root;
  std::filesystem::path git_dir;
  std::filesystem::path hooks_dir;
};

TEST_F(InstallTest, InstallCreatesHookFile) {
  auto fs = std::make_unique<infrastructure::OsFileSystem>();
  auto shell_for_git = std::make_unique<infrastructure::ProcessShellExecutor>();
  auto git_repo = std::make_unique<infrastructure::CliGitRepository>(std::move(shell_for_git));

  application::InstallUseCase install_use_case(std::move(fs), std::move(git_repo));

  domain::config::ProjectConfig cfg;
  cfg.project_name = "test";
  cfg.generate_local_hook = true;

  auto result = install_use_case.execute(test_root, cfg);

  EXPECT_TRUE(result.hook_installed);

  auto hook_path = hooks_dir / "pre-commit";
  EXPECT_TRUE(std::filesystem::exists(hook_path));

  std::ifstream file(hook_path);
  std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  EXPECT_FALSE(content.empty());
  EXPECT_NE(content.find("sniffercommit"), std::string::npos);
}
