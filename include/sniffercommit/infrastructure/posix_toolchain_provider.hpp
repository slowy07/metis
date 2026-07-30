#ifndef SNIFFERCOMMIT_INFRASTRUCTURE_POSIX_TOOLCHAIN_PROVIDER_HPP
#define SNIFFERCOMMIT_INFRASTRUCTURE_POSIX_TOOLCHAIN_PROVIDER_HPP

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

#include "sniffercommit/domain/ports/shell_executor.hpp"
#include "sniffercommit/domain/ports/toolchain_provider.hpp"

namespace sniffercommit::infrastructure {

class PosixToolchainProvider : public domain::ports::IToolchainProvider {
 public:
  PosixToolchainProvider(domain::ports::IShellExecutor* shell, std::string compiler,
                         std::string version = "");

  [[nodiscard]] bool is_installed() const override;
  [[nodiscard]] std::optional<std::string> get_version() const override;
  [[nodiscard]] domain::ports::ToolchainPackage resolve_package() const override;
  [[nodiscard]] domain::ports::ToolchainInstallResult install(
      const std::filesystem::path& archive_path) override;
  [[nodiscard]] std::string description() const override;

 private:
  domain::ports::IShellExecutor* shell_;
  std::string compiler_;
  std::string version_;

  [[nodiscard]] bool is_macos() const;
  [[nodiscard]] bool has(std::string_view cmd) const;

  // ponytail: three package-manager buckets: Debian/derivatives,
  // RPM-based (dnf, zypper), Arch (pacman), macOS (brew, port).
  // Enough for the >95% case; exotic distros go in the Unknown branch.
  enum class PkgMgr : std::uint8_t { Unknown, Apt, Dnf, Pacman, Zypper, Brew, Port };
  [[nodiscard]] PkgMgr detect() const;
  [[nodiscard]] std::string install_cmd(PkgMgr pm) const;
  [[nodiscard]] std::string pkg_name(PkgMgr pm) const;
  [[nodiscard]] std::string bin_path(PkgMgr pm) const;
};

}  // namespace sniffercommit::infrastructure

#endif
