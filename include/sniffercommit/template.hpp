#ifndef SNIFFERCOMMIT_TEMPLATE_HPP
#define SNIFFERCOMMIT_TEMPLATE_HPP

#include <string>
namespace sniffercommit {

enum class FormatterStyle { Google, LLVM, Chromium, Mozilla, WebKit, Microsoft, GNU };

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
};


[[nodiscard]] std::string default_sniffercommit_config(const std::string& project_name, const ClangFormatConfig& clang_cfg);
[[nodiscard]] std::string generate_clang_format(const ClangFormatConfig& cfg);

std::string formatter_style_name(FormatterStyle style);

FormatterStyle parse_formatter_style(const std::string& style);
}  // namespace sniffercommit

#endif  // !SNIFFERCOMMIT_TEMPLATE_HPP
