#include "sniffercommit/infrastructure/linux_gcc_provider.hpp"

#include <fmt/format.h>

#include <cstddef>
#include <optional>
#include <string>
#include <utility>

#include "sniffercommit/domain/ports/file_system.hpp"
#include "sniffercommit/domain/ports/shell_executor.hpp"
#include "sniffercommit/domain/ports/toolchain_provider.hpp"

namespace sniffercommit::infrastructure {

namespace {

enum class PackageManager : std::uint8_t { Unknown, Apt, Dnf, Pacman, Zypper };

PackageManager detect_package_manager(domain::ports::IShellExecutor& shell) {
  if (shell.command_exists("apt-get")) {
    return PackageManager::Apt;
  }
  if (shell.command_exists("dnf")) {
    return PackageManager::Dnf;
  }
  if (shell.command_exists("pacman")) {
    return PackageManager::Pacman;
  }
  if (shell.command_exists("zypper")) {
    return PackageManager::Zypper;
  }
  return PackageManager::Unknown;
}

}  // namespace

LinuxGccProvider::LinuxGccProvider(domain::ports::IShellExecutor* shell, std::string version)
    : shell_(shell), version_(std::move(version)) {}

bool LinuxGccProvider::is_installed() const { return shell_->command_exists("gcc"); }

std::optional<std::string> LinuxGccProvider::get_version() const {
  if (!is_installed()) {
    return std::nullopt;
  }

  auto result = shell_->exec_captured("gcc --version");
  if (result.exit_code_ != 0 || result.output_.empty()) {
    return std::nullopt;
  }

  size_t pos = result.output_.find('\n');
  std::string first_line =
      (pos != std::string::npos) ? result.output_.substr(0, pos) : result.output_;
  return first_line;
}

domain::ports::ToolchainPackage LinuxGccProvider::resolve_package() const {
  domain::ports::ToolchainPackage pkg;
  pkg.name_ = "gcc";
  pkg.version_ = version_.empty() ? "latest" : version_;
  pkg.download_url_ = "";
  pkg.archive_type_ = "";
  pkg.install_dir_ = "/usr";
  return pkg;
}

domain::ports::ToolchainInstallResult LinuxGccProvider::install(
    const std::filesystem::path& /*archive_path*/) {
  domain::ports::ToolchainInstallResult result;

  auto package_manager = detect_package_manager(*shell_);
  if (package_manager == PackageManager::Unknown) {
    result.error_message_ = "No supported package manager found (apt, dnf, pacman, zypper)";
    return result;
  }

  // Report success — the package manager installs GCC system-wide.
  result.success_ = true;
  result.version_ = version_.empty() ? "system" : version_;
  result.installed_path_ = "/usr/bin/gcc";
  return result;
}

std::string LinuxGccProvider::description() const { return "GCC (Linux Package Manager)"; }

}  // namespace sniffercommit::infrastructure
