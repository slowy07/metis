#ifndef METIS_DOMAIN_PORTS_DEPENDENCY_PARSER_HPP
#define METIS_DOMAIN_PORTS_DEPENDENCY_PARSER_HPP

#include <filesystem>
#include <string>
#include <vector>

#include "metis/domain/dependency.hpp"

namespace metis::domain::ports {
struct IDependencyParser {
  virtual ~IDependencyParser() = default;

  [[nodiscard]] virtual bool can_parse(const std::filesystem::path& repo_root) const = 0;

  [[nodiscard]] virtual std::vector<Dependency> parse(
      const std::filesystem::path& repo_root) const = 0;

  [[nodiscard]] virtual std::string source_name() const = 0;
};
}  // namespace metis::domain::ports

#endif  // !METIS_DOMAIN_PORTS_DEPENDENCY_PARSER_HPP
