#ifndef SNIFFERCOMMIT_DOMAIN_PORTS_TOOLCHAIN_PROVIDER_HPP
#define SNIFFERCOMMIT_DOMAIN_PORTS_TOOLCHAIN_PROVIDER_HPP

#include <emmintrin.h>

#include <filesystem>
#include <optional>
#include <string>
namespace sniffercommit::domain::ports {
struct ToolchainPackage {
  std::string name_;
  std::string version_;
  std::string download_url_;
  std::string checksum_;
  std::string archive_type_;
  std::string install_dir_;
};

struct ToolchainInstallResult {
  bool success_ = false;
  std::string installed_path_;
  std::string version_;
  std::string error_message_;
};

struct IToolchainProvider {
  virtual ~IToolchainProvider() = default;

  [[nodiscard]] virtual bool is_installed() const = 0;
  [[nodiscard]] virtual std::optional<std::string> get_version() const = 0;
  [[nodiscard]] virtual ToolchainPackage resolve_package() const = 0;
  [[nodiscard]] virtual ToolchainInstallResult install(
      const std::filesystem::path& archive_parh) = 0;
  [[nodiscard]] virtual std::string description() const = 0;
};
}  // namespace sniffercommit::domain::ports

#endif  // !SNIFFERCOMMIT_DOMAIN_PORTS_TOOLCHAIN_PROVIDER_HPP
