#ifndef SNIFFERCOMMIT_TEMPLATE_HPP
#define SNIFFERCOMMIT_TEMPLATE_HPP

#include <string>
namespace sniffercommit {

enum class FormatterStyle { Google, LLVM, Chromium, Mozilla, WebKit, Microsoft, GNU };


[[nodiscard]] std::string default_sniffercommit_config(const std::string& project_name);
[[nodiscard]] std::string default_clang_format(FormatterStyle style);

FormatterStyle parse_formatter_style(const std::string& style);
}  // namespace sniffercommit

#endif  // !SNIFFERCOMMIT_TEMPLATE_HPP
