#ifndef SNIFFERCOMMIT_DOMAIN_PORTS_TOOLCHAIN_PROVIDER_HPP
#define SNIFFERCOMMIT_DOMAIN_PORTS_TOOLCHAIN_PROVIDER_HPP

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace sniffercommit::domain::ports {

enum class CppStandard : std::uint8_t {
  CPP_17 = 17,
  CPP_20 = 20,
  CPP_23 = 23,
  CPP_26 = 26,
};

struct ToolchainPackage {
  std::string name_;
  std::string version_;
  std::string download_url_;
  std::string checksum_;
  std::string archive_type_;
  std::string install_dir_;
  CppStandard cpp_standard_ = CppStandard::CPP_20;
};

struct ToolchainInstallResult {
  bool success_ = false;
  std::string installed_path_;
  std::string version_;
  std::string error_message_;
  CppStandard installed_cpp_standard_ = CppStandard::CPP_20;
};

struct IToolchainProvider {
  virtual ~IToolchainProvider() = default;

  [[nodiscard]] virtual bool is_installed() const = 0;
  [[nodiscard]] virtual std::optional<std::string> get_version() const = 0;
  [[nodiscard]] virtual ToolchainPackage resolve_package() const = 0;
  [[nodiscard]] virtual ToolchainInstallResult install(
      const std::filesystem::path& archive_path) = 0;
  [[nodiscard]] virtual std::string description() const = 0;

  [[nodiscard]] virtual bool supports_cpp_standard(CppStandard standard) const = 0;
  [[nodiscard]] virtual CppStandard max_supported_standard() const = 0;
};
}  // namespace sniffercommit::domain::ports

#endif  // !SNIFFERCOMMIT_DOMAIN_PORTS_TOOLCHAIN_PROVIDER_HPP
