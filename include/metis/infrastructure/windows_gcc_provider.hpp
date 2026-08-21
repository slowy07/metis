#ifndef METIS_INFRASTRUCTURE_WINDOWS_GCC_PROVIDER_HPP
#define METIS_INFRASTRUCTURE_WINDOWS_GCC_PROVIDER_HPP

#include <optional>
#include <string>

#include "metis/domain/ports/file_system.hpp"
#include "metis/domain/ports/shell_executor.hpp"
#include "metis/domain/ports/toolchain_provider.hpp"

namespace metis::infrastructure {
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

  [[nodiscard]] bool supports_cpp_standard(domain::ports::CppStandard standard) const override;
  [[nodiscard]] domain::ports::CppStandard max_supported_standard() const override;

 private:
  domain::ports::IShellExecutor* shell_;
  domain::ports::IFileSystem* fs_;
  std::string version_;
  std::filesystem::path install_prefix_;

  [[nodiscard]] std::filesystem::path default_install_prefix() const;
  [[nodiscard]] std::string build_download_url() const;
  [[nodiscard]] int parse_major_version() const;
};
}  // namespace metis::infrastructure

#endif  // !METIS_INFRASTRUCTURE_WINDOWS_GCC_PROVIDER_HPP
