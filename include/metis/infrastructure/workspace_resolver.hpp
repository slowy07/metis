#ifndef METIS_INFRASTRUCTURE_WORKSPACE_RESOLVER_HPP
#define METIS_INFRASTRUCTURE_WORKSPACE_RESOLVER_HPP

#include <filesystem>
#include <optional>

#include "metis/domain/config.hpp"

namespace metis::infrastructure {
class WorkspaceResolver {
 public:
  explicit WorkspaceResolver(std::filesystem::path start);

  [[nodiscard]] std::optional<std::filesystem::path> find_root() const;

 private:
  std::filesystem::path start_;
};
}  // namespace metis::infrastructure

#endif  // !METIS_INFRASTRUCTURE_WORKSPACE_RESOLVER_HPP
