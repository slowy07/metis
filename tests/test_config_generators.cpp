#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "metis/domain/config.hpp"

using namespace metis::domain::config;

TEST(ConfigGeneratorTest, DefaultConfigContainsProjectName) {
  auto result = generate_default_config("my-proj");
  EXPECT_NE(result.find("name = \"my-proj\""), std::string::npos);
}

TEST(ConfigGeneratorTest, DefaultConfigContainsClangFormatCheck) {
  auto result = generate_default_config("test");
  EXPECT_NE(result.find("clang-format"), std::string::npos);
}

TEST(ConfigGeneratorTest, DefaultConfigContainsTrailingWhitespaceCheck) {
  auto result = generate_default_config("test");
  EXPECT_NE(result.find("trailing-whitespace"), std::string::npos);
}

TEST(ConfigGeneratorTest, DefaultConfigWithTidyAddsTidyCheck) {
  auto result = generate_default_config_with_tidy("test", "Google", "standard", ".");
  EXPECT_NE(result.find("clang-tidy"), std::string::npos);
  EXPECT_NE(result.find("--config-file="), std::string::npos);
}

TEST(ConfigGeneratorTest, CompilerChecksDefaultSingleCheck) {
  auto result = generate_compiler_checks("g++", "20", {"Wall", "Wextra"}, true, false);
  EXPECT_NE(result.find("compiler-g++-20"), std::string::npos);
  EXPECT_NE(result.find("-std=c++20"), std::string::npos);
  EXPECT_NE(result.find("-Wall"), std::string::npos);
  EXPECT_NE(result.find("-Werror"), std::string::npos);
}

TEST(ConfigGeneratorTest, CompilerChecksDebugAndRelease) {
  auto result = generate_compiler_checks("g++", "20", {}, false, true);
  EXPECT_NE(result.find("compiler-g++-debug"), std::string::npos);
  EXPECT_NE(result.find("compiler-g++-release"), std::string::npos);
  EXPECT_NE(result.find("-O0"), std::string::npos);
  EXPECT_NE(result.find("-O2"), std::string::npos);
}

TEST(ConfigGeneratorTest, SanitizerConfigDefaultTypes) {
  auto result = generate_sanitizer_config();
  EXPECT_NE(result.find("[sanitizers]"), std::string::npos);
  EXPECT_NE(result.find("enabled = true"), std::string::npos);
  EXPECT_NE(result.find("\"address\""), std::string::npos);
  EXPECT_NE(result.find("\"undefined\""), std::string::npos);
}

TEST(ConfigGeneratorTest, SanitizerConfigCustomTypes) {
  auto result = generate_sanitizer_config({"thread", "leak"}, "build/", 30);
  EXPECT_NE(result.find("\"thread\""), std::string::npos);
  EXPECT_NE(result.find("\"leak\""), std::string::npos);
  EXPECT_NE(result.find("build_dir = \"build/\""), std::string::npos);
  EXPECT_NE(result.find("timeout = 30"), std::string::npos);
}

TEST(ConfigValidationTest, EmptyProjectNameFails) {
  ProjectConfig cfg;
  cfg.project_name = "";
  cfg.checks.push_back(Check{.name = "test", .command = "echo"});
  EXPECT_FALSE(cfg.validate().empty());
}

TEST(ConfigValidationTest, NoChecksFails) {
  ProjectConfig cfg;
  cfg.project_name = "test";
  EXPECT_FALSE(cfg.validate().empty());
}

TEST(ConfigValidationTest, DuplicateCheckNamesFail) {
  ProjectConfig cfg;
  cfg.project_name = "test";
  cfg.checks.push_back(Check{.name = "dup", .command = "echo"});
  cfg.checks.push_back(Check{.name = "dup", .command = "echo"});
  EXPECT_FALSE(cfg.validate().empty());
}

TEST(ConfigValidationTest, ValidConfigPasses) {
  ProjectConfig cfg;
  cfg.project_name = "test";
  cfg.checks.push_back(Check{.name = "ok", .command = "echo"});
  EXPECT_TRUE(cfg.validate().empty());
}

TEST(ConfigValidationTest, HasCommandFindsMatch) {
  ProjectConfig cfg;
  cfg.project_name = "test";
  cfg.checks.push_back(Check{.name = "fmt", .command = "clang-format"});
  EXPECT_TRUE(cfg.has_command("clang-format"));
  EXPECT_FALSE(cfg.has_command("clang-tidy"));
}
