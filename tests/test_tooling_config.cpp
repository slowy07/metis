#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "metis/generators/clang_format_generator.hpp"
#include "metis/generators/clang_tidy_generator.hpp"
#include "metis/generators/cmake_generator.hpp"

using namespace metis::generators;

TEST(ClangFormatStyleTest, StyleNameReturnsCorrectStrings) {
  EXPECT_EQ(generate_clang_format_style("google"), "Google");
  EXPECT_EQ(generate_clang_format_style("GOOGLE"), "Google");
  EXPECT_EQ(generate_clang_format_style("llvm"), "LLVM");
  EXPECT_EQ(generate_clang_format_style("chromium"), "Chromium");
  EXPECT_EQ(generate_clang_format_style("mozilla"), "Mozilla");
  EXPECT_EQ(generate_clang_format_style("webkit"), "WebKit");
  EXPECT_EQ(generate_clang_format_style("microsoft"), "Microsoft");
  EXPECT_EQ(generate_clang_format_style("gnu"), "GNU");
}

TEST(ClangFormatStyleTest, InvalidStyleThrows) {
  EXPECT_THROW((void)generate_clang_format_style("invalid"), std::runtime_error);
}

TEST(ClangFormatTest, GeneratesValidYaml) {
  std::string content = generate_clang_format("google", 2, 100, "Left", "Attach");
  EXPECT_NE(content.find("BasedOnStyle: Google"), std::string::npos);
  EXPECT_NE(content.find("IndentWidth: 2"), std::string::npos);
  EXPECT_NE(content.find("ColumnLimit: 100"), std::string::npos);
}

TEST(ClangFormatTest, InvalidConfigThrows) {
  EXPECT_THROW((void)generate_clang_format("google", 0, 100, "Left", "Attach"), std::runtime_error);
  EXPECT_THROW((void)generate_clang_format("google", 2, 10, "Left", "Attach"), std::runtime_error);
  EXPECT_THROW((void)generate_clang_format("google", 17, 100, "Left", "Attach"),
               std::runtime_error);
}

TEST(ClangTidyTest, MinimalPresetIncludesCoreGuidelines) {
  auto checks = get_preset_checks("minimal");
  EXPECT_FALSE(checks.empty());

  bool found_cppcore = false;
  for (const auto& check : checks) {
    if (check.find("cppcoreguidelines-") != std::string::npos) {
      found_cppcore = true;
    }
  }
  EXPECT_TRUE(found_cppcore);
}

TEST(ClangTidyTest, StandardPresetIncludesMoreChecks) {
  auto minimal = get_preset_checks("minimal");
  auto standard = get_preset_checks("standard");
  EXPECT_GT(standard.size(), minimal.size());
}

TEST(ClangTidyTest, StrictPresetIncludesMostChecks) {
  auto standard = get_preset_checks("standard");
  auto strict = get_preset_checks("strict");
  EXPECT_GT(strict.size(), standard.size());
}

TEST(ClangTidyTest, CustomPresetIsEmpty) {
  auto checks = get_preset_checks("custom");
  EXPECT_TRUE(checks.empty());
}

TEST(ClangTidyTest, InvalidPresetThrows) {
  EXPECT_THROW((void)get_preset_checks("invalid"), std::runtime_error);
}

TEST(ClangTidyTest, GenerateClangTidy) {
  std::string content = generate_clang_tidy("standard", "error", 1);
  EXPECT_NE(content.find("Checks:"), std::string::npos);
  EXPECT_NE(content.find("WarningsAsErrors:"), std::string::npos);
}

TEST(CMakeTest, GenerateCMakeLists) {
  std::string content = generate_cmake_lists("test-project", "20", "executable", false, false, true,
                                             false, false, {});
  EXPECT_NE(content.find("cmake_minimum_required(VERSION 3.20)"), std::string::npos);
  EXPECT_NE(content.find("project(test-project"), std::string::npos);
  EXPECT_NE(content.find("CMAKE_CXX_STANDARD 20"), std::string::npos);
  EXPECT_NE(content.find("add_executable(test-project"), std::string::npos);
}

TEST(CMakeTest, GenerateCMakeListsWithTesting) {
  std::string content =
      generate_cmake_lists("test", "17", "static", true, false, true, false, false, {});
  EXPECT_NE(content.find("enable_testing()"), std::string::npos);
  EXPECT_NE(content.find("add_library(test STATIC"), std::string::npos);
  EXPECT_NE(content.find("CMAKE_CXX_STANDARD 17"), std::string::npos);
}
