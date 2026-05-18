#include "sniffercommit/tooling_config.hpp"

#include <fmt/format.h>

#include <stdexcept>
#include <string>
#include <vector>

namespace sniffercommit::tooling {

std::string ClangFormatConfig::validate() const noexcept {
  if (ident_width < 1 || ident_width > 16) {
    return "IndentWidth must be between 1 and 16";
  }

  if (column_limit < 20 || column_limit > 500) {
    return "ColumnLimit must be between 20 and 500";
  }

  return "";
}

std::string style_name(FormatterStyle style) {
  switch (style) {
    case FormatterStyle::Google:
      return "Google";
    case FormatterStyle::LLVM:
      return "LLVM";
    case FormatterStyle::Chromium:
      return "Chromium";
    case FormatterStyle::Mozilla:
      return "Mozilla";
    case FormatterStyle::WebKit:
      return "WebKit";
    case FormatterStyle::Microsoft:
      return "Microsoft";
    case FormatterStyle::GNU:
      return "GNU";
  }
  return "Google";
}

FormatterStyle parse_style(const std::string& style) {
  std::string lower;

  for (char c : style) {
    lower += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }

  if (lower == "google") return FormatterStyle::Google;
  if (lower == "llvm") return FormatterStyle::LLVM;
  if (lower == "chromium") return FormatterStyle::Chromium;
  if (lower == "mozilla") return FormatterStyle::Mozilla;
  if (lower == "webkit") return FormatterStyle::WebKit;
  if (lower == "microsoft") return FormatterStyle::Microsoft;
  if (lower == "gnu") return FormatterStyle::GNU;

  throw std::runtime_error("Unknown formatter style: " + style);
}

std::string generate_clang_format(const ClangFormatConfig& cfg) {
  if (auto err = cfg.validate(); !err.empty()) {
    throw std::runtime_error("Invalid clang format config: " + err);
  }

  auto bool_str = [](bool v) { return v ? "true" : "false"; };

  return fmt::format(
      R"(---
BasedOnStyle: {}

IndentWidth: {}
ColumnLimit: {}

PointerAlignment: {}
BreakBeforeBraces: {}

Standard: {}

SortIncludes: {}
ReflowComments: {}
AlignConsecutiveAssignments: {}

...
)",
      style_name(cfg.style), cfg.ident_width, cfg.column_limit, cfg.pointer_alignment,
      cfg.break_before_braces, cfg.standard, bool_str(cfg.sort_includes),
      bool_str(cfg.reflow_comments), bool_str(cfg.align_consecutive_assignments));
}

// NOTE: clang tidy logic
std::string ClangTidyConfig::validate() const noexcept {
  if (preset == TidyPreset::Custom && checks.empty()) {
    return "Custom preset requires at least one explicit checks";
  }

  if (header_filter_level < 0 || header_filter_level > 2) {
    return "header_filter_level must be 0 (none), 1 (project), or 2 (all)";
  }

  return "";
}

std::string preset_name(TidyPreset preset) {
  switch (preset) {
    case TidyPreset::Minimal:
      return "minimal";
    case TidyPreset::Standard:
      return "standard";
    case TidyPreset::Strict:
      return "strict";
    case TidyPreset::Custom:
      return "custom";
  }

  return "standard";
}

std::string severity_name(TidySeverity severity) {
  switch (severity) {
    case TidySeverity::Note:
      return "note";
    case TidySeverity::Warning:
      return "warning";
    case TidySeverity::Error:
      return "error";
  }
  return "error";
}

std::vector<std::string> preset_checks(TidyPreset preset) {
  switch (preset) {
    case TidyPreset::Minimal:
      return {
          "cppcoreguidelines-*",
          "bugprone-*",
          "clang-analyzer-*",
          "--cppcoreguidelines-avoid-magic-numbers",
          "-cppcoreguidelines-pre-bounds-array-to-pointer-decay",
      };
    case TidyPreset::Standard:
      return {
          "cppcoreguidelines-*",
          "bugprone-*",
          "clang-analyzer-*",
          "modernize-*",
          "performance-*",
          "-cppcoreguidelines-avoid-magic-numbers",
          "-cppcoreguidelines-pro-bounds-array-to-pointer-decay",
          "-modernize-use-trailing-return-type",
      };

    case TidyPreset::Strict:
      return {
          "*",
          "-abseil-*",
          "-altera-*",
          "-fuchsia-*",
          "-llvm-*",
          "-zircon-*",
          "-google-readability-todo",
          "-readability-identifier-length",
          "-cppcoreguidelines-avoid-magic-numbers",
          "-cppcoreguidelines-pro-bounds-array-to-pointer-decay",
          "-modernize-use-trailing-return-type",
      };

    case TidyPreset::Custom:
      return {};
  }

  return preset_checks(TidyPreset::Standard);
}

std::string generate_clang_tidy(const ClangTidyConfig& cfg) {
  if (auto err = cfg.validate(); !err.empty()) {
    throw std::runtime_error("invalid clang-tidy config: " + err);
  }

  std::vector<std::string> active_checks;

  if (cfg.preset != TidyPreset::Custom) {
    active_checks = preset_checks(cfg.preset);
  } else {
    active_checks = cfg.checks;
  }

  // appending extra checks
  for (const auto& c : cfg.extra_checks) {
    active_checks.push_back(c);
  }

  for (const auto& c : cfg.exclude_checks) {
    active_checks.push_back("-" + c);
  }

  std::string check_str;
  for (size_t i = 0; i < active_checks.size(); ++i) {
    if (i > 0) {
      check_str += ",";
    }

    check_str += active_checks[i];
  }

  std::string header_filter;

  switch (cfg.header_filter_level) {
    case 0:
      header_filter += "\"\"";
      break;
    case 1:
      header_filter += "\".*\"";
      break;
    case 2:
      header_filter += "\".*\"";
      break;
    default:
      header_filter += "\".*\"";
      break;
  }

  auto bool_str = [](bool v) { return v ? "true" : "false"; };
  return fmt::format(
      R"(---
      # Generated by sniffercommit v0.2.1 | DO NOT EDIT manually
      # Preset: {}
      # Severity threshold: {}
      
      Checks: "{}"
      WarningsAsErrors: "{}"
      headerFilterRegex: {}
      FormatStyle: {}
      Quiet: {}
      
      CheckOptions:
        - key: readability-identifier-naming.NamespaceCase
          value: lower_case
        - key: readability-identifier-naming.ClassCase
          value: CamelCase
        - key: readability-identifier-naming.FunctionCase
          value: camelBack
        - key: readability-identifier-naming.VariabelCase
          value: lower_case
        - key: readability-identifier-naming.MemberCase
          value: lower_case
        - key: readability-identifier-naming.MemberSuffix
          value: _
        - key: cppcoreguidelines-special-member-functions.AllowSoleDefaultDtor
          value: 1
        - key: cppcoreguidelines-avoid-magic-numbers.IgnorePowersOf2
          value: 1
        - key: modernize-loop-convert.MaxCopySize
          value: 16
        - key: modernize-pass-by-value.IncludeStyle
          value: llvm
        - key: modernize-use-default-member-init.UseAssignment
          value: 1
        - key: performance-unnecessary-value-param.AllowedTypes
          value: "std::.*;llvm::.*;fmt::.*"
        - key: bugprone-exception-escape.FunctionThatShouldNotThrow
          value: "main;wmain;WinMain;wWinMain"
        - key: readibility-function-cognitive-complexity.Threshold
          value: 25
        - key: readibility-function-cognitive-complexity.DescribeBasicIncrements
          value: 0

      ...
      )",
      preset_name(cfg.preset), severity_name(cfg.warnings_as_errors), check_str,
      severity_name(cfg.warnings_as_errors), header_filter, cfg.format_style ? "file" : "none",
      bool_str(cfg.quiet));
}

}  // namespace sniffercommit::tooling
