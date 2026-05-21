#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include "../include/sniffercommit/config_manager.hpp"
#include "sniffercommit/tooling_config.hpp"

using namespace sniffercommit;

class ConfigManagerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    test_dir = std::filesystem::temp_directory_path() / "sniffercommit_config_test";
    std::filesystem::create_directories(test_dir);
  }

  void TearDown() override { std::filesystem::remove_all(test_dir); }

  std::filesystem::path test_dir;
};

TEST_F(ConfigManagerTest, WriteFileCreateFile) {
  auto file_path = test_dir / "test.txt";
  std::string content = "woilah cik";

  bool result = ConfigManager::write_file(file_path, content);

  EXPECT_TRUE(result);
  EXPECT_TRUE(std::filesystem::exists(file_path));

  std::ifstream file(file_path);
  std::string read_content((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
  EXPECT_EQ(read_content, content);
}

TEST_F(ConfigManagerTest, InitializeWithClangTidyCreateFileTidy) {
  ConfigManager::InitOptions opts;
  opts.project_name = "test-project";
  opts.style = tooling::FormatterStyle::Google;
  opts.enable_clang_tidy = true;
  opts.tidy_preset = tooling::TidyPreset::Standard;
}
