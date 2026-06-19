#ifndef SNIFFERCOMMIT_DOMAIN_PORTS_CONFIG_REPOSITORY_HPP
#define SNIFFERCOMMIT_DOMAIN_PORTS_CONFIG_REPOSITORY_HPP

#include <filesystem>
#include <string>

#include "sniffercommit/domain/config.hpp"

namespace sniffercommit::domain::ports {

struct IConfigRepository {
  virtual ~IConfigRepository() = default;

  [[nodiscard]] virtual domain::config::ProjectConfig load(const std::filesystem::path& path) = 0;
  [[nodiscard]] virtual bool save(const std::filesystem::path& path,
                                  const domain::config::ProjectConfig& cfg) = 0;
  [[nodiscard]] virtual std::filesystem::path find_git_root() = 0;
};

}  // namespace sniffercommit::domain::ports

#endif
