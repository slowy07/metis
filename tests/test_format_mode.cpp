#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>

#include "metis/application/run_checks_use_case.hpp"
#include "metis/domain/config.hpp"
#include "metis/domain/error_codes.hpp"
#include "metis/infrastructure/cli_git_repository.hpp"
#include "metis/infrastructure/os_file_system.hpp"
#include "metis/infrastructure/process_shell_executor.hpp"

using namespace metis;

class FormatModeTest : public ::testing::Test {
 protected:
  std::filesystem::path temp_dir_;
  std::filesystem::path repo_root_;
  std::filesystem::path orig_cwd_;

  void SetUp() override {
    orig_cwd_ = std::filesystem::current_path();
    temp_dir_ = std::filesystem::temp_directory_path() / "metis_test_format";
    repo_root_ = temp_dir_ / "repo";
    std::filesystem::create_directories(repo_root_);
    std::filesystem::create_directories(repo_root_ / ".git");
    std::filesystem::create_directories(repo_root_ / "src");

    {
      std::ofstream file(repo_root_ / "src" / "main.cpp");
      file << "int main() {\n";
      file << "    int x=1;\n";
      file << "return 0;\n";
      file << "}\n";
    }

    (void)std::system(("cd " + repo_root_.string() + " && git init >/dev/null 2>&1").c_str());
    (void)std::system(
        ("cd " + repo_root_.string() + " && git config user.email test@test.com >/dev/null 2>&1")
            .c_str());
    (void)std::system(
        ("cd " + repo_root_.string() + " && git config user.name test >/dev/null 2>&1").c_str());
    (void)std::system(("cd " + repo_root_.string() + " && git add . >/dev/null 2>&1").c_str());
    (void)std::system(
        ("cd " + repo_root_.string() + " && git commit -m 'init' >/dev/null 2>&1").c_str());

    std::filesystem::current_path(repo_root_);
  }

  void TearDown() override {
    std::filesystem::current_path(orig_cwd_);
    std::filesystem::remove_all(temp_dir_);
  }
};

TEST_F(FormatModeTest, RunChecksWithEmptyConfig) {
  auto shell_for_git = std::make_unique<infrastructure::ProcessShellExecutor>();
  auto git_repo = std::make_unique<infrastructure::CliGitRepository>(std::move(shell_for_git));
  auto shell = std::make_unique<infrastructure::ProcessShellExecutor>();
  auto fs = std::make_unique<infrastructure::OsFileSystem>();

  application::RunChecksUseCase use_case(std::move(shell), std::move(git_repo), std::move(fs));

  domain::config::ProjectConfig cfg;
  cfg.project_name = "test";

  application::RunOptions opts;
  opts.source = application::FileSource::ALL_REPO;
  opts.mode = application::RunMode::CHECK;

  int result = use_case.execute(cfg, opts);
  EXPECT_EQ(result, 0);
}

TEST_F(FormatModeTest, DryRunFormat) {
  auto shell_for_git = std::make_unique<infrastructure::ProcessShellExecutor>();
  auto git_repo = std::make_unique<infrastructure::CliGitRepository>(std::move(shell_for_git));
  auto shell = std::make_unique<infrastructure::ProcessShellExecutor>();
  auto fs = std::make_unique<infrastructure::OsFileSystem>();

  application::RunChecksUseCase use_case(std::move(shell), std::move(git_repo), std::move(fs));

  domain::config::ProjectConfig cfg;
  cfg.project_name = "test";

  application::RunOptions opts;
  opts.source = application::FileSource::ALL_REPO;
  opts.mode = application::RunMode::FORMAT;
  opts.dry_run = true;

  int result = use_case.execute(cfg, opts);

  // Without a .clang-format file, expect CONFIG_ERROR
  EXPECT_EQ(result, static_cast<int>(domain::ExitCode::CONFIG_ERROR));
}
