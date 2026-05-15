#include <stdexcept>
#include <string>

#include "fmt/format.h"
#include "sniffercommit/template.hpp"

namespace sniffercommit {
std::string default_sniffercommit_config() {
  return R"([project]
name = "my-project"

[[checks]]
name = "clang-format"
command = "clang-format"
args = ["-i", "--fallback-style=Google", "-style=file"]
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
)";
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

std::string default_clang_format(FormatterStyle style) {
  return fmt::format(R"(---
BasedOnStyle: {}

IndentWidth: 2
ColumnLimit: 100
PointerAlignment: Left
SortIncludes: true

AllowShortFunctionsOnASingleLine: Empty
BreakBeforeBraces: Attach

Standard: Latest
...
)",
                     formatter_style_name(style));
}

}  // namespace sniffercommit
