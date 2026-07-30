#include "sniffercommit/infrastructure/macos_clang_provider.hpp"

#include <fmt/format.h>

namespace sniffercommit::infrastructure {

MacosClangProvider::MacosClangProvider(domain::ports::IShellExecutor* shell, std::string version)
    : shell_(shell), version_(std::move(version)) {}

bool MacosClangProvider::is_installed() const { return shell_->command_exists("clang"); }

std::optional<std::string> MacosClangProvider::get_version() const {
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

domain::ports::ToolchainPackage MacosClangProvider::resolve_package() const {
  domain::ports::ToolchainPackage pkg;
  pkg.name_ = "clang";
  pkg.version_ = version_.empty() ? "latest" : version_;
  pkg.download_url_ = "";
  pkg.archive_type_ = "";
  pkg.install_dir_ = "/usr";
  return pkg;
}

domain::ports::ToolchainInstallResult MacosClangProvider::install(
    const std::filesystem::path& /*archive_path*/) {
  domain::ports::ToolchainInstallResult result;

  // Try Homebrew first
  if (has_homebrew()) {
    auto exec_result = shell_->exec_captured("brew install llvm");
    if (exec_result.exit_code_ != 0) {
      result.error_message_ =
          fmt::format("Homebrew failed (exit {}): {}", exec_result.exit_code_, exec_result.output_);
      return result;
    }
    result.success_ = true;
    result.installed_path_ = "/opt/homebrew/opt/llvm/bin/clang";
    auto ver = get_version();
    result.version_ = ver.value_or("unknown");
    return result;
  }

  if (has_macports()) {
    auto exec_result = shell_->exec_captured("sudo port install clang");
    if (exec_result.exit_code_ != 0) {
      result.error_message_ =
          fmt::format("MacPorts failed (exit {}): {}", exec_result.exit_code_, exec_result.output_);
      return result;
    }
    result.success_ = true;
    result.installed_path_ = "/opt/local/bin/clang";
    auto ver = get_version();
    result.version_ = ver.value_or("unknown");
    return result;
  }

  result.error_message_ =
      "No package manager found (Homebrew or MacPorts).\\n"
      "Please install Xcode Command Line Tools manually:\\n"
      "  xcode-select --install";
  return result;
}

std::string MacosClangProvider::description() const { return "Clang (macOS Homebrew/MacPorts)"; }

bool MacosClangProvider::has_homebrew() const { return shell_->command_exists("brew"); }

bool MacosClangProvider::has_macports() const { return shell_->command_exists("port"); }

}  // namespace sniffercommit::infrastructure
