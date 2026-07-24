#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include "sniffercommit/infrastructure/os_file_system.hpp"

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
