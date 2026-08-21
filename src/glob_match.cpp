#include "metis/glob_match.hpp"

#include <algorithm>

// Simple glob pattern matching for file paths.
// Supports three patterns:
//   *.ext     — matches files with the given extension
//   prefix/** — matches files under a directory
//   **/suffix — matches files ending with the suffix
//   exact     — exact string match
//   prefix    — matches files starting with the prefix
// lazy: doesn't support ? or [character classes]. Nobody has asked for them.
[[nodiscard]] bool metis::util::glob_match(std::string_view file,
                                                   std::string_view pattern) noexcept {
  if (pattern == "") {
    return true;
  }

  if (pattern == "*") {
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

// Returns true if the file matches any of the given patterns.
// Empty pattern list means "match everything".
[[nodiscard]] bool metis::util::matches_any_pattern(
    const std::string& file, const std::vector<std::string>& patterns) {
  if (patterns.empty()) {
    return true;
  }

  return std::ranges::any_of(patterns,
                             [&file](const auto& patt) { return glob_match(file, patt); });
}
