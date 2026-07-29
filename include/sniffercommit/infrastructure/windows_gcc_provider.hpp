#ifndef SNIFFERCOMMIT_INFRASTRUCTURE_WINDOWS_GCC_PROVIDER_HPP
#define SNIFFERCOMMIT_INFRASTRUCTURE_WINDOWS_GCC_PROVIDER_HPP

#include <optional>
#include <string>

#include "sniffercommit/domain/ports/file_system.hpp"
#include "sniffercommit/domain/ports/shell_executor.hpp"
#include "sniffercommit/domain/ports/toolchain_provider.hpp"

namespace sniffercommit::infrastructure {
class WindowsGccProvider : public domain::ports::IToolchainProvider {
 public:
  WindowsGccProvider(domain::ports::IShellExecutor* shell, domain::ports::IFileSystem* fs,
                     std::string version = "");

  [[nodiscard]] bool is_installed() const override;
  [[nodiscard]] std::optional<std::string> get_version() const override;
  [[nodiscard]] domain::ports::ToolchainPackage resolve_package() const override;
  [[nodiscard]] domain::ports::ToolchainInstallResult install(
      const std::filesystem::path& archive_path) override;
  [[nodiscard]] std::string description() const override;

 private:
  domain::ports::IShellExecutor* shell_;
  domain::ports::IFileSystem* fs_;
  std::string version_;
  std::filesystem::path install_prefix_;

  [[nodiscard]] std::filesystem::path default_install_prefix() const;
  [[nodiscard]] std::string build_download_url() const;
};
}  // namespace sniffercommit::infrastructure

#endif  // !SNIFFERCOMMIT_INFRASTRUCTURE_WINDOWS_GCC_PROVIDER_HPP
