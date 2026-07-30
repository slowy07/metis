#include <fmt/format.h>

#include "sniffercommit/infrastructure/windows_clang_provider.hpp"

namespace sniffercommit::infrastructure {
WindowsClangProvider::WindowsClangProvider(domain::ports::IShellExecutor* shell,
                                           domain::ports::IFileSystem* fs, std::string version)
    : shell_(shell),
      fs_(fs),
      version_(version.empty() ? "19.1.0" : version),
      install_prefix_(default_install_prefix()) {}

bool WindowsClangProvider::is_installed() const {
  return shell_->command_exists("clang") || shell_->command_exists("clang.exe");
}

std::optional<std::string> WindowsClangProvider::get_version() const {
  if (!is_installed()) {
    return std::nullopt;
  }
  auto result = shell_->exec_captured("clang --version");
  if (result.exit_code_ != 0 || result.output_.empty()) {
    return std::nullopt;
  }
  size_t pos = result.output_.find('\n');
  return (pos != std::string::npos) ? result.output_.substr(0, pos) : result.output_;
}

domain::ports::ToolchainPackage WindowsClangProvider::resolve_package() const {
  domain::ports::ToolchainPackage pkg;
  pkg.name_ = "llvm-clang";
  pkg.version_ = version_;
  pkg.download_url_ = build_download_url();
  pkg.checksum_ = "";
  pkg.archive_type_ = "zip";
  pkg.install_dir_ = install_prefix_.string();
  return pkg;
}

domain::ports::ToolchainInstallResult WindowsClangProvider::install(
    const std::filesystem::path& archive_path) {
  domain::ports::ToolchainInstallResult result;

  if (archive_path.empty()) {
    result.error_message_ = "No archive provided for extraction";
    return result;
  }

  if (!fs_->create_directories(install_prefix_)) {
    result.error_message_ = "Failed to create installation directory";
    return result;
  }

  auto clang_bin = install_prefix_ / "bin" / "clang.exe";
  if (!fs_->exists(clang_bin)) {
    result.error_message_ = fmt::format(
        "Extraction completed but clang.exe not found at expected path: {}. "
        "The archive structure may have changed.",
        clang_bin.string());
    return result;
  }

  result.success_ = true;
  result.installed_path_ = install_prefix_.string();
  auto ver = get_version();
  result.version_ = ver.value_or(version_);
  return result;
}

std::string WindowsClangProvider::description() const { return "LLVM/Clang (GitHub Releases)"; }

std::filesystem::path WindowsClangProvider::default_install_prefix() const {
  const char* home = std::getenv("USERPROFILE");
  if (home == nullptr) {
    home = std::getenv("HOME");
  }
  if (home == nullptr) {
    return std::filesystem::path("C:\\") / "sniffercommit" / "toolchains" /
           fmt::format("llvm-{}", version_);
  }
  return std::filesystem::path(home) / ".sniffercommit" / "toolchains" /
         fmt::format("llvm-{}", version_);
}

std::string WindowsClangProvider::build_download_url() const {
  return fmt::format(
      "https://github.com/llvm/llvm-project/releases/download/llvmorg-{}/"
      "LLVM-{}-win64.zip",
      version_, version_);
}
}  // namespace sniffercommit::infrastructure
