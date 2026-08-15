#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <memory>

#include "sniffercommit/domain/config.hpp"
#include "sniffercommit/infrastructure/os_file_system.hpp"
#include "sniffercommit/infrastructure/process_shell_executor.hpp"
#include "sniffercommit/infrastructure/toml_config_repository.hpp"

using namespace sniffercommit;

class FileSystemTest : public ::testing::Test {
 protected:
  void SetUp() override {
    test_dir = std::filesystem::temp_directory_path() / "sniffercommit_fs_test";
    std::filesystem::create_directories(test_dir);
  }

  void TearDown() override { std::filesystem::remove_all(test_dir); }

  std::filesystem::path test_dir;
};

TEST_F(FileSystemTest, WriteFileCreateFile) {
  infrastructure::OsFileSystem fs;
  auto file_path = test_dir / "test.txt";
  std::string content = "woilah cik";

  bool result = fs.write_file(file_path, content);

  EXPECT_TRUE(result);
  EXPECT_TRUE(std::filesystem::exists(file_path));

  std::ifstream file(file_path);
  std::string read_content((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
  EXPECT_EQ(read_content, content);
}

TEST_F(FileSystemTest, ReadFileReturnsContent) {
  infrastructure::OsFileSystem fs;
  auto file_path = test_dir / "read_test.txt";
  std::string content = "hello world";

  ASSERT_TRUE(fs.write_file(file_path, content));
  auto result = fs.read_file(file_path);

  EXPECT_EQ(result, content);
}

TEST_F(FileSystemTest, ExistsDetectsFile) {
  infrastructure::OsFileSystem fs;
  auto file_path = test_dir / "exist_test.txt";

  EXPECT_FALSE(fs.exists(file_path));

  ASSERT_TRUE(fs.write_file(file_path, "content"));
  EXPECT_TRUE(fs.exists(file_path));
}

TEST_F(FileSystemTest, RemoveDeletesFile) {
  infrastructure::OsFileSystem fs;
  auto file_path = test_dir / "remove_test.txt";

  ASSERT_TRUE(fs.write_file(file_path, "content"));
  EXPECT_TRUE(fs.exists(file_path));

  ASSERT_TRUE(fs.remove(file_path));
  EXPECT_FALSE(fs.exists(file_path));
}

// Generic check abstraction: full field set parses from TOML.
TEST(ConfigCheckTest, ParsesAllCheckFields) {
  auto cfg_dir = std::filesystem::temp_directory_path() / "sniffercommit_check_test";
  std::filesystem::create_directories(cfg_dir);
  auto cfg_path = cfg_dir / ".sniffercommit.toml";
  infrastructure::OsFileSystem fs;
  ASSERT_TRUE(fs.write_file(cfg_path,
                            "[project]\n"
                            "name = \"demo\"\n\n"
                            "[[checks]]\n"
                            "name = \"clang-format\"\n"
                            "description = \"Format C++ files\"\n"
                            "enabled = false\n"
                            "command = \"clang-format\"\n"
                            "args = [\"-i\", \"-style=file\"]\n"
                            "patterns = [\"*.cpp\", \"*.hpp\"]\n"
                            "timeout = 30\n"
                            "severity = \"warning\"\n"));

  infrastructure::TomlConfigRepository repo(
      std::make_unique<infrastructure::OsFileSystem>(),
      std::make_unique<infrastructure::ProcessShellExecutor>());
  auto cfg = repo.load(cfg_path);

  ASSERT_EQ(cfg.checks.size(), 1u);
  const auto& check = cfg.checks.at(0);
  EXPECT_EQ(check.name, "clang-format");
  EXPECT_EQ(check.description, "Format C++ files");
  EXPECT_FALSE(check.enabled);
  EXPECT_EQ(check.command, "clang-format");
  ASSERT_EQ(check.args.size(), 2u);
  EXPECT_EQ(check.args.at(0), "-i");
  ASSERT_EQ(check.patterns.size(), 2u);
  EXPECT_EQ(check.patterns.at(1), "*.hpp");
  EXPECT_EQ(check.timeout, 30);
  EXPECT_EQ(check.severity, "warning");
}

// Generic check abstraction: execute() runs command + args via the shell.
TEST(ConfigCheckTest, ExecuteRunsCommand) {
  domain::config::Check check;
  check.command = "echo";
  check.args = {"hello"};

  infrastructure::ProcessShellExecutor shell;
  auto result = check.execute(shell, {"a.cpp"});

  EXPECT_EQ(result.exit_code_, 0);
  EXPECT_NE(result.output_.find("hello"), std::string::npos);
}
