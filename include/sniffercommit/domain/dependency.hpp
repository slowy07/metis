#ifndef SNIFFERCOMMIT_DOMAIN_DEPEDENCY_HPP
#define SNIFFERCOMMIT_DOMAIN_DEPEDENCY_HPP

#include <algorithm>
#include <string>
#include <vector>

namespace sniffercommit::domain {
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
  std::vector<std::string> missing_files;
  std::vector<std::string> lockfile_issues;

  [[nodiscard]] bool success() const noexcept {
    if (!missing_files.empty()) {
      return false;
    }

    if (!lockfile_issues.empty()) {
      return false;
    }

    return std::ranges::all_of(validations,
                               [](const auto& valid) { return valid.ok; });
  }
};

}  // namespace sniffercommit::domain

#endif  // !SNIFFERCOMMIT_DOMAIN_DEPEDENCY_HPP
