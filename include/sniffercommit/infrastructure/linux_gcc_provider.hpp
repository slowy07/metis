#ifndef SNIFFERCOMMIT_INFRASTRUCTURE_LINUX_GCC_PROVIDER_HPP
#define SNIFFERCOMMIT_INFRASTRUCTURE_LINUX_GCC_PROVIDER_HPP

#include <memory>
#include <string>

#include "sniffercommit/domain/ports/file_system.hpp"
#include "sniffercommit/domain/ports/shell_executor.hpp"
#include "sniffercommit/domain/ports/toolchain_provider.hpp"

namespace sniffercommit::infrastructure {

class LinuxGccProvider : public domain::ports::IToolchainProvider {
 public:
  LinuxGccProvider(domain::ports::IShellExecutor* shell, std::string version = "");

  [[nodiscard]] bool is_installed() const override;
  [[nodiscard]] std::optional<std::string> get_version() const override;
  [[nodiscard]] domain::ports::ToolchainPackage resolve_package() const override;
  [[nodiscard]] domain::ports::ToolchainInstallResult install(
      const std::filesystem::path& archive_path) override;
  [[nodiscard]] std::string description() const override;

 private:
  domain::ports::IShellExecutor* shell_;
  std::string version_;
};
}  // namespace sniffercommit::infrastructure

#endif  // !SNIFFERCOMMIT_INFRASTRUCTURE_LINUX_GCC_PROVIDER_HPP
