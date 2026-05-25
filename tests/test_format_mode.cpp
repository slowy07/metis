#include <gtest/gtest.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>

#include "sniffercommit/executor.hpp"

namespace sniffercommit {
class FormatModeTest : public ::testing::Test {
 protected:
  std::filesystem::path temp_dir_;
  std::filesystem::path repo_root_;

  void SetUp() override {
    temp_dir_ = std::filesystem::temp_directory_path() / "sniffercommit_test_format";
    repo_root_ = temp_dir_ / "repo";
    std::filesystem::create_directories(repo_root_);
    std::filesystem::create_directories(repo_root_ / ".git");
    std::filesystem::create_directories(repo_root_ / "src");

    // make clang
    {
      std::ofstream file(repo_root_ / ".clang-format");
      file << "BasedOnly: Google\n";
      file << "IndentWidth: 2\n";
      file << "ColumnLimit: 100\n";
    }

    {
      std::ofstream file(repo_root_ / "src" / "main.cpp");
      file << "int main() {\n";
      file << "    int x=1;\n";
      file << "return 0;\n";
      file << "}\n";
    }

    std::system(("cd " + repo_root_.string() + " && git init >/dev/null 2>&1").c_str());
    std::system(("cd " + repo_root_.string() + " && git add . >/dev/null 2>&1").c_str());
    std::system(("cd " + repo_root_.string() + " && git commit -m 'init' >/dev/null 2>&1").c_str());
  }

  void TearDown() override { std::filesystem::remove_all(temp_dir_); }
};

TEST_F(FormatModeTest, FormatEligibleFile) {
  RunOptions opts;
  opts.source = FileSource::ALL_REPO;
  opts.mode = RunMode::FORMAT;

  auto files = collect_files(repo_root_, opts, {});
  ASSERT_FALSE(files.empty());

  EXPECT_TRUE(std::ranges::any_of(
      files, [](const auto& file_dat) { return file_dat.ends_with("main.cpp"); }));
}

TEST_F(FormatModeTest, DryRunFormat) {
  RunOptions opts;
  opts.source = FileSource::ALL_REPO;
  opts.mode = RunMode::FORMAT;

  opts.dry_run = true;
  auto files = collect_files(repo_root_, opts, {});
  int result = execute_format(repo_root_, files, opts);

  EXPECT_EQ(result, 0);
}

TEST_F(FormatModeTest, FormatModeRespectExplicitFiles) {
  RunOptions opts;
  opts.source = FileSource::EXPLICIT;
  opts.mode = RunMode::FORMAT;
  opts.explicit_files = {"src/main.cpp"};

  auto files = collect_files(repo_root_, opts, {});
  ASSERT_EQ(files.size(), 1);
  EXPECT_TRUE(files[0].ends_with("main.cpp"));
}

}  // namespace sniffercommit
