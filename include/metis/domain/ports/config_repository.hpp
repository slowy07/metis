#ifndef METIS_DOMAIN_PORTS_CONFIG_REPOSITORY_HPP
#define METIS_DOMAIN_PORTS_CONFIG_REPOSITORY_HPP

#include <filesystem>
#include <string>

#include "metis/domain/config.hpp"

namespace metis::domain::ports {

// Interface for config file persistence.
// lazy: only one implementation (TomlConfigRepository). The interface
// exists for potential test mocking, but could be simplified to a
// concrete class if testability isn't needed.
struct IConfigRepository {
  virtual ~IConfigRepository() = default;

  [[nodiscard]] virtual domain::config::ProjectConfig load(const std::filesystem::path& path) = 0;
  [[nodiscard]] virtual bool save(const std::filesystem::path& path,
                                  const domain::config::ProjectConfig& cfg) = 0;
  [[nodiscard]] virtual std::filesystem::path find_git_root() = 0;
};

}  // namespace metis::domain::ports

#endif
