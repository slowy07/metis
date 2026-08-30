#ifndef METIS_DOMAIN_DEPENDENCY_VERSION_CHECKER_HPP
#define METIS_DOMAIN_DEPENDENCY_VERSION_CHECKER_HPP

#include <optional>
#include <string>

namespace metis::domain::ports {
struct IDependencyVersionChecker {
  virtual ~IDependencyVersionChecker() = default;

  [[nodiscard]] virtual std::optional<std::string> latest_version(
      const std::string& package_name) const = 0;

  [[nodiscard]] virtual std::string source_name() const = 0;
};
}  // namespace metis::domain::ports

#endif  // !METIS_DOMAIN_DEPENDENCY_VERSION_CHECKER_HPP
