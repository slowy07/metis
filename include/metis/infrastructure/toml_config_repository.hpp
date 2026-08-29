#ifndef METIS_INFRASTRUCTURE_TOML_CONFIG_REPOSITORY_HPP
#define METIS_INFRASTRUCTURE_TOML_CONFIG_REPOSITORY_HPP

#include <filesystem>
#include <memory>

#include "metis/domain/config.hpp"
#include "metis/domain/ports/file_system.hpp"
#include "metis/domain/ports/shell_executor.hpp"

namespace metis::infrastructure {

class TomlConfigRepository {
 public:
  TomlConfigRepository(std::unique_ptr<domain::ports::IFileSystem> fs,
                       std::unique_ptr<domain::ports::IShellExecutor> shell);

  [[nodiscard]] domain::config::ProjectConfig load(const std::filesystem::path& path);
  [[nodiscard]] std::filesystem::path find_git_root();

 private:
  std::unique_ptr<domain::ports::IFileSystem> fs_;
  std::unique_ptr<domain::ports::IShellExecutor> shell_;
};

}  // namespace metis::infrastructure

#endif
