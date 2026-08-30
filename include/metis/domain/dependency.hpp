#ifndef METIS_DOMAIN_DEPENDENCY_HPP
#define METIS_DOMAIN_DEPENDENCY_HPP

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

namespace metis::domain {
struct Dependency {
  std::string name;
  std::string version;
  // INFO: currently conan, vcpkg, cmake-fetchcontent
  std::string source;

  std::optional<std::string> latest_version;
  bool has_update = false;

  [[nodiscard]] bool is_outdated() const noexcept {
    return has_update && latest_version.has_value() && latest_version.value() != version;
  }
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
  std::vector<Dependency> outdated;

  [[nodiscard]] bool success() const noexcept {
    if (!lockfile_issues.empty()) {
      return false;
    }

    return std::ranges::all_of(validations, [](const auto& valid) { return valid.ok; });
  }

  [[nodiscard]] bool has_outdated() const noexcept { return !outdated.empty(); }
};

struct DependencyManageResult {
  bool success = false;
  std::vector<std::string> messages;
  std::vector<std::string> modified_files;

  void add_message(std::string msg) { messages.push_back(std::move(msg)); }

  void add_modified(std::string file) { modified_files.push_back(std::move(file)); }
};

}  // namespace metis::domain

#endif  // !METIS_DOMAIN_DEPEDENCY_HPP
