#ifndef SNIFFERCOMMIT_GENERATORS_CLANG_TIDY_GENERATOR_HPP
#define SNIFFERCOMMIT_GENERATORS_CLANG_TIDY_GENERATOR_HPP

#include <string>
#include <string_view>
#include <vector>

namespace sniffercommit::generators {

[[nodiscard]] std::string generate_clang_tidy(std::string_view preset, std::string_view severity,
                                              int header_filter_level);

[[nodiscard]] std::vector<std::string> get_preset_checks(std::string_view preset);

}  // namespace sniffercommit::generators

#endif
