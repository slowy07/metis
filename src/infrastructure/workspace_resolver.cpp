#include "metis/infrastructure/workspace_resolver.hpp"

#include <filesystem>
#include <optional>
#include <toml++/toml.hpp>
#include <utility>

namespace metis::infrastructure {
WorkspaceResolver::WorkspaceResolver(std::filesystem::path start)
  : start_(std::move(start)) {}

std::optional<std::filesystem::path> WorkspaceResolver::find_root() const {
  auto current = std::filesystem::absolute(start_);

  while (true) {
    auto config_path = current / ".metis.toml";
    if (std::filesystem::exists(config_path)) {
      try {
        auto tbl = toml::parse_file(config_path.string());
        if (tbl["workspace"].is_table()) {
          return current;
        }
      } catch (...) {
        // TODO: better catch error
      }
    }

    auto parent = current.parent_path();
    if (parent == current) {
      break;
    }
    current = parent;
  }

  return std::nullopt;
}
}  // namespace metis::infrastructure
