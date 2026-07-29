#include "sniffercommit/infrastructure/windows_gcc_provider.hpp"

#include <fmt/format.h>

#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <string>

#include "sniffercommit/domain/ports/file_system.hpp"
#include "sniffercommit/domain/ports/shell_executor.hpp"
#include "sniffercommit/domain/ports/toolchain_provider.hpp"

namespace sniffercommit::infrastructure {
WindowsGccProvider::WindowsGccProvider(domain::ports::IShellExecutor& shell,
                                       domain::ports::IFileSystem& fs, std::string version)
    : shell_(shell),
      fs_(fs),
      version_(version.empty() ? "14.2.0" : version),
      install_prefix_(default_install_prefix()) {}

bool WindowsGccProvider::is_installed() const {
  return shell_.command_exists("gcc") || shell_.command_exists("gcc.exe");
}

std::optional<std::string> WindowsGccProvider::get_version() const {
  if (!is_installed()) {
    return std::nullopt;
  }

  auto result = shell_.exec_captured("gcc --version");
  if (result.exit_code_ != 0 || result.output_.empty()) {
    return std::nullopt;
  }

  size_t pos = result.output_.find('\n');
  return (pos != std::string::npos) ? result.output_.substr(0, pos) : result.output_;
}

domain::ports::ToolchainPackage WindowsGccProvider::resolve_package() const {
  domain::ports::ToolchainPackage pkg;
  pkg.name_ = "mingw-w64";
  pkg.version_ = version_;
  pkg.download_url_ = build_download_url();

  // TODO: add SHA-256 verification
  pkg.checksum_ = "";
  pkg.archive_type_ = "zip";
  pkg.install_dir_ = install_prefix_.string();
  return pkg;
}

domain::ports::ToolchainInstallResult WindowsGccProvider::install(
    const std::filesystem::path& archive_path) {
  domain::ports::ToolchainInstallResult result;

  if (archive_path.empty()) {
    result.error_message_ = "No archive provided for extraction";
    return result;
  }

  if (!fs_.create_directories(install_prefix_)) {
    result.error_message_ = "Failed to create installation directory";
    return result;
  }

  auto gcc_bin = install_prefix_ / "mingw64" / "bin" / "gcc.exe";
  if (!fs_.exists(gcc_bin)) {
    result.error_message_ = fmt::format(
        "Extraction completed but gcc.exe not found at expected path: {}. "
        "The archive structure may have changed.",
        gcc_bin.string());
    return result;
  }

  result.success_ = true;
  result.installed_path_ = install_prefix_.string();

  auto ver = get_version();
  result.version_ = ver.value_or(version_);
  return result;
}

std::string WindowsGccProvider::description() const { return "MinGW-w64 (WinLibs)"; }

std::filesystem::path WindowsGccProvider::default_install_prefix() const {
  const char* home = std::getenv("USERPROFILE");

  if (home == nullptr) {
    home = std::getenv("HOME");
  }

  if (home == nullptr) {
    return std::filesystem::path("C:\\") / "sniffercommit" / "toolchains" /
           fmt::format("mingw-{}", version_);
  }
  return std::filesystem::path(home) / ".sniffercommit" / "toolchains" /
         fmt::format("mingw-{}", version_);
}

std::string WindowsGccProvider::build_download_url() const {
  return fmt::format(
      "https://github.com/brechtsanders/winlibs_mingw/releases/download/"
      "{}posix-19.1.1-ucrt-r2/"
      "winlibs-x86_64-posix-seh-gcc-{}-llvm-19.1.1-mingw-w64ucrt-12.0.0-r2.zip",
      version_, version_);
}
}  // namespace sniffercommit::infrastructure
