#ifndef METIS_GLOB_MATCH_HPP
#define METIS_GLOB_MATCH_HPP

#include <string>
#include <string_view>
#include <vector>
namespace metis::util {
[[nodiscard]] bool glob_match(std::string_view file, std::string_view pattern) noexcept;
[[nodiscard]] bool matches_any_pattern(const std::string& file,
                                       const std::vector<std::string>& patterns);
}  // namespace metis::util

#endif  // !METIS_GLOB_MATCH_HPP
