#include "sniffercommit/generators/clang_format_generator.hpp"

#include <fmt/format.h>

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string>

namespace sniffercommit::generators {

std::string generate_clang_format_style(std::string_view style) {
  std::string lower;
  for (char chr : style) {
    lower += static_cast<char>(std::tolower(static_cast<unsigned char>(chr)));
  }

  if (lower == "google") return "Google";
  if (lower == "llvm") return "LLVM";
  if (lower == "chromium") return "Chromium";
  if (lower == "mozilla") return "Mozilla";
  if (lower == "webkit") return "WebKit";
  if (lower == "microsoft") return "Microsoft";
  if (lower == "gnu") return "GNU";
  throw std::runtime_error("Unknown formatter style: " + std::string(style));
}

std::string generate_clang_format(std::string_view style, int indent_width, int column_limit,
                                  std::string_view pointer_alignment,
                                  std::string_view brace_style) {
  std::string style_name = generate_clang_format_style(style);

  auto indent_ok = (indent_width >= 1 && indent_width <= 16);
  auto column_ok = (column_limit >= 20 && column_limit <= 500);
  if (!indent_ok || !column_ok) {
    throw std::runtime_error("Invalid clang-format config values");
  }

  return fmt::format(
      R"(---
BasedOnStyle: {}

IndentWidth: {}
ColumnLimit: {}

PointerAlignment: {}
BreakBeforeBraces: {}

Standard: Cpp11

SortIncludes: true
ReflowComments: true
AlignConsecutiveAssignments: false

...
)",
      style_name, indent_width, column_limit, pointer_alignment, brace_style);
}

}  // namespace sniffercommit::generators
