#include "sniffercommit/glob_match.hpp"

#include <algorithm>

[[nodiscard]] bool sniffercommit::util::glob_match(std::string_view file,
                                                          std::string_view pattern) noexcept {
  if (pattern == "") {
    return true;
  }

  if (pattern.starts_with("*.") && file.ends_with(pattern.substr(1))) {
    return true;
  }

  if (pattern.ends_with("/**") && file.starts_with(pattern.substr(0, pattern.size() - 3))) {
    return true;
  }

  if (pattern.starts_with("**/")) {
    return file.ends_with(pattern.substr(3));
  }

  return file == pattern || file.starts_with(pattern);
}

[[nodiscard]] bool sniffercommit::util::matches_any_pattern(
    const std::string& file, const std::vector<std::string>& patterns) {
  if (patterns.empty()) {
    return true;
  }

  return std::ranges::any_of(patterns,
                             [&file](const auto& patt) { return glob_match(file, patt); });
}
