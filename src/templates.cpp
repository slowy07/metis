#include <fmt/format.h>

#include <stdexcept>
#include <string>

#include "sniffercommit/template.hpp"

namespace sniffercommit {
std::string default_sniffercommit_config(const std::string& project_name,
                                         const ClangFormatConfig& clang_cfg) {
  return fmt::format(
      R"([project]
name = "{}"

[[checks]]
name = "clang-format"
command = "clang-format"
args = [
  "-i", 
  "--fallback-style={}", 
  "-style=file"
]
patterns = ["*.cpp", "*.hpp", "*.h", "*.cc"]

[[checks]]
name = "trailing-whitespace"
command = "grep"
args = ["-E", "--text", "[[:space:]]+$"]
patterns = ["*"]

[exclude]
paths = ["build/", "third_party/", ".git/"]

[output]
local_hook = true
github_actions = false

[execution]
parallel = true
)",
      project_name, formatter_style_name(clang_cfg.style));
}

std::string bool_to_yaml(bool value) {
  return value ? "true" : "false";
}

std::string formatter_style_name(FormatterStyle style) {
  switch (style) {
    case sniffercommit::FormatterStyle::Google:
      return "Google";

    case sniffercommit::FormatterStyle::LLVM:
      return "LLVM";

    case sniffercommit::FormatterStyle::Chromium:
      return "Chromium";

    case sniffercommit::FormatterStyle::Mozilla:
      return "Mozilla";

    case sniffercommit::FormatterStyle::WebKit:
      return "WebKit";

    case sniffercommit::FormatterStyle::Microsoft:
      return "Microsoft";

    case sniffercommit::FormatterStyle::GNU:
      return "GNU";

    default:
      return "Google";
  }

  return "Google";
}

FormatterStyle parse_formatter_style(const std::string& style) {
  if (style == "google") {
    return FormatterStyle::Google;
  }

  if (style == "llvm") {
    return FormatterStyle::LLVM;
  }

  if (style == "chromium") {
    return FormatterStyle::Chromium;
  }

  if (style == "mozilla") {
    return FormatterStyle::Mozilla;
  }

  if (style == "webkit") {
    return FormatterStyle::WebKit;
  }

  if (style == "microsoft") {
    return FormatterStyle::Microsoft;
  }

  if (style == "gnu") {
    return FormatterStyle::GNU;
  }

  throw std::runtime_error("[ERROR] unknown formatter style " + style);
}

std::string generate_clang_format(const ClangFormatConfig& cfg) {
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
      formatter_style_name(cfg.style), cfg.ident_width, cfg.column_limit, cfg.pointer_alignment,
      cfg.break_before_braces, cfg.standard, bool_to_yaml(cfg.sort_includes),
      bool_to_yaml(cfg.reflow_comments), bool_to_yaml(cfg.align_consecutive_assignments));
}

}  // namespace sniffercommit
