#include "sniffercommit/tooling_config.hpp"

#include <fmt/format.h>

#include <stdexcept>
#include <string>

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

}  // namespace sniffercommit::tooling
