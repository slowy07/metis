#ifndef METIS_DOMAIN_DEPENDENCY_HPP
#define METIS_DOMAIN_DEPENDENCY_HPP

#include <algorithm>
#include <string>
#include <vector>

namespace metis::domain {
struct Dependency {
  std::string name;
  std::string version;
  // INFO: currently conan, vcpkg, cmake-fetchcontent
  std::string source;
};

struct DependencyValidation {
  Dependency dep;
  bool ok = true;
  std::string message;
};

struct DependencyCheckResult {
  std::vector<DependencyValidation> validations;
  std::vector<std::string> duplicates;
  std::vector<std::string> lockfile_issues;

  [[nodiscard]] bool success() const noexcept {
    if (!lockfile_issues.empty()) {
      return false;
    }

    return std::ranges::all_of(validations, [](const auto& valid) { return valid.ok; });
  }
};

}  // namespace metis::domain

#endif  // !METIS_DOMAIN_DEPEDENCY_HPP
