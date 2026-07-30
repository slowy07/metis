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

LinuxGccProvider::LinuxGccProvider(domain::ports::IShellExecutor* shell, std::string compiler,
                                   std::string version)
    : shell_(shell), compiler_(std::move(compiler)), version_(std::move(version)) {}

bool LinuxGccProvider::is_installed() const { return shell_->command_exists(compiler_.c_str()); }

std::optional<std::string> LinuxGccProvider::get_version() const {
  if (!is_installed()) {
    return std::nullopt;
  }

  auto result = shell_->exec_captured(compiler_ + " --version");
  if (result.exit_code_ != 0 || result.output_.empty()) {
    return std::nullopt;
  }

  size_t pos = result.output_.find('\n');
  return (pos != std::string::npos) ? result.output_.substr(0, pos) : result.output_;
}

domain::ports::ToolchainPackage LinuxGccProvider::resolve_package() const {
  domain::ports::ToolchainPackage pkg;
  pkg.name_ = compiler_;
  pkg.version_ = version_.empty() ? "latest" : version_;
  pkg.download_url_ = "";
  pkg.archive_type_ = "";
  pkg.install_dir_ = "/usr";
  return pkg;
}

domain::ports::ToolchainInstallResult LinuxGccProvider::install(
    const std::filesystem::path& /*archive_path*/) {
  domain::ports::ToolchainInstallResult result;

  auto pm = detect_package_manager();
  if (pm == PackageManager::Unknown) {
    result.error_message_ = "No supported package manager found (apt, dnf, pacman, zypper)";
    return result;
  }

  std::string cmd = install_command(pm);
  auto exec_result = shell_->exec_captured(cmd);

  if (exec_result.exit_code_ != 0) {
    result.error_message_ = fmt::format(
        "Package manager failed (exit {}): {}\n"
        "You may need to run with sudo or install manually.",
        exec_result.exit_code_, exec_result.output_);
    return result;
  }

  result.success_ = true;
  result.installed_path_ = "/usr/bin/" + compiler_;
  auto ver = get_version();
  result.version_ = ver.value_or("unknown");
  return result;
}

std::string LinuxGccProvider::description() const {
  return fmt::format("{} (Linux Package Manager)", compiler_);
}

LinuxGccProvider::PackageManager LinuxGccProvider::detect_package_manager() const {
  if (shell_->command_exists("apt-get")) {
    return PackageManager::Apt;
  }
  if (shell_->command_exists("dnf")) {
    return PackageManager::Dnf;
  }
  if (shell_->command_exists("pacman")) {
    return PackageManager::Pacman;
  }
  if (shell_->command_exists("zypper")) {
    return PackageManager::Zypper;
  }
  return PackageManager::Unknown;
}

std::string LinuxGccProvider::install_command(PackageManager pm) const {
  std::string pkg = package_name(pm);
  switch (pm) {
    case PackageManager::Apt:
      return fmt::format("sudo apt-get update && sudo apt-get install -y {}", pkg);
    case PackageManager::Dnf:
      return fmt::format("sudo dnf install -y {}", pkg);
    case PackageManager::Pacman:
      return fmt::format("sudo pacman -S --noconfirm {}", pkg);
    case PackageManager::Zypper:
      return fmt::format("sudo zypper install -y {}", pkg);
    default:
      return {};
  }
}

std::string LinuxGccProvider::package_name(PackageManager pm) const {
  switch (pm) {
    case PackageManager::Apt:
    case PackageManager::Dnf:
    case PackageManager::Pacman:
      return compiler_ + " lldb";
    case PackageManager::Zypper:
    default:
      return compiler_;
  }
}

}  // namespace sniffercommit::infrastructure
