#ifndef SNIFFERCOMMIT_TOOLING_CONFIG_HPP
#define SNIFFERCOMMIT_TOOLING_CONFIG_HPP

#include <cstdint>
#include <string>
#include <vector>

namespace sniffercommit::tooling {

enum class FormatterStyle : std::uint8_t { Google, LLVM, Chromium, Mozilla, WebKit, Microsoft, GNU };

struct ClangFormatConfig {
  FormatterStyle style = FormatterStyle::Google;
  int ident_width = 2;
  int column_limit = 100;
  std::string pointer_alignment = "Left";
  std::string break_before_braces = "Attach";
  std::string standard = "c++20";
  bool sort_includes = true;
  bool reflow_comments = true;
  bool align_consecutive_assignments = false;

  [[nodiscard]] std::string validate() const noexcept;
};

// INFO: converting between string and enum
// and generate .clang-format configs content
[[nodiscard]] std::string style_name(FormatterStyle style);
[[nodiscard]] FormatterStyle parse_style(const std::string& style);
[[nodiscard]] std::string generate_clang_format(const ClangFormatConfig& cfg);

// NOTE: severity level clang-tidy checking
// mapping to -warnings-as-errors
enum class TidySeverity : std::uint8_t { Note, Warning, Error };

enum class TidyPreset : std::uint8_t {
  Minimal,   // set to only bug-prone patterns (cppcoreguidelines-*)
  Standard,  // bug + style + modernize (default)
  Strict,    // everything + performance + reability
  Custom,    // user defined
};

struct ClangTidyConfig {
  TidyPreset preset = TidyPreset::Standard;
  std::vector<std::string> checks;
  std::vector<std::string> extra_checks;
  std::vector<std::string> exclude_checks;

  bool fix = false;
  bool fix_errors = false;
  TidySeverity warnings_as_errors = TidySeverity::Error;
  int header_filter_level = 1;  // setting:
                                // 0 -> none
                                // 1 -> project
                                // 2 -> all headers
  bool format_style = true;
  bool quiet = false;

  [[nodiscard]] std::string validate() const noexcept;
};

// conversion
[[nodiscard]] std::string preset_name(TidyPreset preset);
[[nodiscard]] std::string severity_name(TidySeverity severity);

// file generation (clang-tidy)
[[nodiscard]] std::vector<std::string> preset_checks(TidyPreset preset);
std::string generate_clang_tidy(const ClangTidyConfig& cfg);

}  // namespace sniffercommit::tooling

#endif  // !SNIFFERCOMMIT_TOOLING_CONFIG_HPP
