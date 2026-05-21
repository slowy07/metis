#include <gtest/gtest.h>

#include "../include/sniffercommit/tooling_config.hpp"

using namespace sniffercommit::tooling;

TEST(ClangFormatConfigTest, DefaultConfigIsValid) {
  ClangFormatConfig cfg;
  EXPECT_TRUE(cfg.validate().empty());
}

TEST(ClangFormatConfigTest, ValidRejectsOnvalidIndentWidth) {
  ClangFormatConfig cfg;
  cfg.indent_width = 0;

  EXPECT_FALSE(cfg.validate().empty());

  cfg.indent_width = 17;
  EXPECT_FALSE(cfg.validate().empty());
}

TEST(ClangFormatConfigTest, ValidateRejectsInvalidColumnLimit) {
  ClangFormatConfig cfg;
  cfg.column_limit = 15;
  EXPECT_FALSE(cfg.validate().empty());

  cfg.column_limit = 600;
  EXPECT_FALSE(cfg.validate().empty());
}

TEST(ClangFormatConfigTest, ValidConfigValues) {
  ClangFormatConfig cfg;
  cfg.indent_width = 4;
  cfg.column_limit = 120;
  EXPECT_TRUE(cfg.validate().empty());
}

TEST(FormatterStyleTest, StyleNameReturnsCorrectStrings) {
  EXPECT_EQ(style_name(FormatterStyle::Google), "Google");
  EXPECT_EQ(style_name(FormatterStyle::LLVM), "LLVM");
  EXPECT_EQ(style_name(FormatterStyle::Chromium), "Chromium");
  EXPECT_EQ(style_name(FormatterStyle::Mozilla), "Mozilla");
  EXPECT_EQ(style_name(FormatterStyle::WebKit), "WebKit");
  EXPECT_EQ(style_name(FormatterStyle::Microsoft), "Microsoft");
  EXPECT_EQ(style_name(FormatterStyle::GNU), "GNU");
}

TEST(FormatterStyleTest, ParseStyleCaseInsensitive) {
  EXPECT_EQ(parse_style("google"), FormatterStyle::Google);
  EXPECT_EQ(parse_style("GOOGLE"), FormatterStyle::Google);
  EXPECT_EQ(parse_style("llvm"), FormatterStyle::LLVM);
  EXPECT_EQ(parse_style("Chromium"), FormatterStyle::Chromium);
}

TEST(ClangTidyConfigTest, DefaultConfigIsValid) {
  ClangTidyConfig cfg;
  EXPECT_TRUE(cfg.validate().empty());
}

TEST(ClangTidyConfigTest, ValidateHeaderFilterRange) {
  ClangTidyConfig cfg;
  cfg.header_filter_level = -1;
  EXPECT_FALSE(cfg.validate().empty());

  cfg.header_filter_level = 3;
  EXPECT_FALSE(cfg.validate().empty());

  cfg.header_filter_level = 0;
  EXPECT_TRUE(cfg.validate().empty());

  cfg.header_filter_level = 1;
  EXPECT_TRUE(cfg.validate().empty());

  cfg.header_filter_level = 2;
  EXPECT_TRUE(cfg.validate().empty());
}

TEST(ClangTidyConfigTest, ValidateCustomPresetRequiresChecks) {
  ClangTidyConfig cfg;
  cfg.preset = TidyPreset::Custom;
  EXPECT_FALSE(cfg.validate().empty());

  cfg.checks = {"bugprone-*", "modernize-*"};
  EXPECT_TRUE(cfg.validate().empty());
}

TEST(ClangTidyConfigTest, PresetNameReturnsCorrectStrings) {
  EXPECT_EQ(preset_name(TidyPreset::Minimal), "minimal");
  EXPECT_EQ(preset_name(TidyPreset::Standard), "standard");
  EXPECT_EQ(preset_name(TidyPreset::Strict), "strict");
  EXPECT_EQ(preset_name(TidyPreset::Custom), "custom");
}

TEST(ClangTidyConfigTest, SeverityNameReturnsCorrectStrings) {
  EXPECT_EQ(severity_name(TidySeverity::Note), "note");
  EXPECT_EQ(severity_name(TidySeverity::Warning), "warning");
  EXPECT_EQ(severity_name(TidySeverity::Error), "error");
}

TEST(PresetChecksTest, MinimalPresetIncludesCoreGuidelines) {
  auto checks = preset_checks(TidyPreset::Minimal);
  EXPECT_FALSE(checks.empty());

  // NOTE: verify key bug-prone checks are included
  bool found_cppcore = false;
  for (const auto& check : checks) {
    if (check.find("cppcoreguidelines-") != std::string::npos) {
      found_cppcore = true;
    }
  }
  EXPECT_TRUE(found_cppcore);
}

TEST(PresetChecksTest, StandardPresetIncludesMoreChecks) {
  auto minimal = preset_checks(TidyPreset::Minimal);
  auto standard = preset_checks(TidyPreset::Standard);

  EXPECT_GT(standard.size(), minimal.size());
}

TEST(PresetChecksTest, StrictPresetIncludesMostChecks) {
  auto standard = preset_checks(TidyPreset::Standard);
  auto strict = preset_checks(TidyPreset::Strict);

  EXPECT_GT(strict.size(), standard.size());
}

TEST(GenerateClangFormatTest, GeneratesValidYaml) {
    ClangFormatConfig cfg;
    cfg.style = FormatterStyle::Google;
    cfg.indent_width = 2;
    cfg.column_limit = 100;
    
    std::string content = generate_clang_format(cfg);
    
    EXPECT_NE(content.find("BasedOnStyle: Google"), std::string::npos);
    EXPECT_NE(content.find("IndentWidth: 2"), std::string::npos);
    EXPECT_NE(content.find("ColumnLimit: 100"), std::string::npos);
}
