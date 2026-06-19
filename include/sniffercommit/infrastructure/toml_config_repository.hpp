#ifndef SNIFFERCOMMIT_INFRASTRUCTURE_TOML_CONFIG_REPOSITORY_HPP
#define SNIFFERCOMMIT_INFRASTRUCTURE_TOML_CONFIG_REPOSITORY_HPP

#include <filesystem>
#include <memory>

#include "sniffercommit/domain/ports/config_repository.hpp"
#include "sniffercommit/domain/ports/file_system.hpp"
#include "sniffercommit/domain/ports/shell_executor.hpp"

namespace sniffercommit::infrastructure {

class TomlConfigRepository : public domain::ports::IConfigRepository {
 public:
  TomlConfigRepository(std::unique_ptr<domain::ports::IFileSystem> fs,
                       std::unique_ptr<domain::ports::IShellExecutor> shell);

  [[nodiscard]] domain::config::ProjectConfig load(const std::filesystem::path& path) override;
  [[nodiscard]] bool save(const std::filesystem::path& path,
                          const domain::config::ProjectConfig& cfg) override;
  [[nodiscard]] std::filesystem::path find_git_root() override;

 private:
  std::unique_ptr<domain::ports::IFileSystem> fs_;
  std::unique_ptr<domain::ports::IShellExecutor> shell_;
};

}  // namespace sniffercommit::infrastructure

#endif
