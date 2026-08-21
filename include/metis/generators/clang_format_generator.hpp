#ifndef METIS_GENERATORS_CLANG_FORMAT_GENERATOR_HPP
#define METIS_GENERATORS_CLANG_FORMAT_GENERATOR_HPP

#include <string>
#include <string_view>

namespace metis::generators {

[[nodiscard]] std::string generate_clang_format(std::string_view style, int indent_width,
                                                int column_limit,
                                                std::string_view pointer_alignment,
                                                std::string_view brace_style);

[[nodiscard]] std::string generate_clang_format_style(std::string_view style);

}  // namespace metis::generators

#endif
