#include "sniffercommit/infrastructure/posix_toolchain_provider.hpp"

#include <fmt/format.h>

#include <cstddef>
#include <string>
#include <utility>

#include "sniffercommit/domain/ports/shell_executor.hpp"

namespace sniffercommit::infrastructure {

PosixToolchainProvider::PosixToolchainProvider(domain::ports::IShellExecutor* shell,
                                               std::string compiler, std::string version)
    : shell_(shell), compiler_(std::move(compiler)), version_(std::move(version)) {}

bool PosixToolchainProvider::is_installed() const {
  return shell_->command_exists(compiler_.c_str());
}

std::optional<std::string> PosixToolchainProvider::get_version() const {
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

domain::ports::ToolchainPackage PosixToolchainProvider::resolve_package() const {
  domain::ports::ToolchainPackage pkg;
  pkg.name_ = compiler_;
  pkg.version_ = version_.empty() ? "latest" : version_;
  pkg.install_dir_ = "/usr";
  // binaries exist. GCC has no standard binary distribution on POSIX.
  if (compiler_ == "clang" && version_ != "latest" && !version_.empty()) {
    pkg.download_url_ = is_macos()
                            ? fmt::format(
                                  "https://github.com/llvm/llvm-project/releases/download/"
                                  "llvmorg-{}/clang+llvm-{}-arm64-apple-darwin.tar.xz",
                                  version_, version_)
                            : fmt::format(
                                  "https://github.com/llvm/llvm-project/releases/download/"
                                  "llvmorg-{}/clang+llvm-{}-x86_64-linux-gnu-ubuntu-22.04.tar.xz",
                                  version_, version_);
    pkg.archive_type_ = "tar.xz";
  }
  return pkg;
}

domain::ports::ToolchainInstallResult PosixToolchainProvider::install(
    const std::filesystem::path& archive_path) {
  domain::ports::ToolchainInstallResult result;

  // InstallToolchainUseCase. Find the compiler binary inside the tarball
  // root. This heuristic (bin/<compiler>) matches LLVM's official tarball
  // layout. Other layouts need a manifest file.
  if (!archive_path.empty()) {
    auto extracted = archive_path.parent_path();
    for (const auto& entry : std::filesystem::directory_iterator(extracted)) {
      if (!entry.is_directory()) {
        continue;
      }
      auto binary = entry.path() / "bin" / compiler_;
      if (std::filesystem::exists(binary)) {
        result.success_ = true;
        result.installed_path_ = binary.string();
        auto ver = get_version();
        result.version_ = ver.value_or(version_);
        return result;
      }
    }
    result.error_message_ =
        fmt::format("Extracted archive does not contain {}/bin/{}", extracted.string(), compiler_);
    return result;
  }

  // install-mode: use system package manager
  auto pm = detect();
  if (pm == PkgMgr::Unknown) {
    result.error_message_ = is_macos()
                                ? "No supported package manager found (Homebrew or MacPorts).\n"
                                  "  Install Xcode Command Line Tools: xcode-select --install"
                                : "No supported package manager found (apt, dnf, pacman, zypper)";
    return result;
  }

  auto cmd = install_cmd(pm);
  auto exec_result = shell_->exec_captured(cmd);
  if (exec_result.exit_code_ != 0) {
    result.error_message_ = fmt::format(
        "Package manager failed (exit {}): {}\n"
        "You may need to run with sudo or install manually.",
        exec_result.exit_code_, exec_result.output_);
    return result;
  }

  result.success_ = true;
  result.installed_path_ = bin_path(pm);
  auto ver = get_version();
  result.version_ = ver.value_or("unknown");
  return result;
}

std::string PosixToolchainProvider::description() const {
  return fmt::format("{} (POSIX)", compiler_);
}

bool PosixToolchainProvider::supports_cpp_standard(domain::ports::CppStandard standard) const {
  return static_cast<int>(standard) <= static_cast<int>(max_supported_standard());
}

domain::ports::CppStandard PosixToolchainProvider::max_supported_standard() const {
  auto major = parse_major_version();
  if (major == 0) {
    return domain::ports::CppStandard::CPP_23;
  }
  if (compiler_ == "gcc") {
    if (major >= 15) {
      return domain::ports::CppStandard::CPP_26;
    }
    if (major >= 14) {
      return domain::ports::CppStandard::CPP_23;
    }
    if (major >= 11) {
      return domain::ports::CppStandard::CPP_20;
    }
    return domain::ports::CppStandard::CPP_17;
  }
  if (major >= 19) {
    return domain::ports::CppStandard::CPP_26;
  }
  if (major >= 17) {
    return domain::ports::CppStandard::CPP_23;
  }
  if (major >= 14) {
    return domain::ports::CppStandard::CPP_20;
  }
  return domain::ports::CppStandard::CPP_17;
}

int PosixToolchainProvider::parse_major_version() const {
  if (version_.empty() || version_ == "latest") {
    return 0;
  }
  try {
    return std::stoi(version_);
  } catch (...) {
    auto dot = version_.find('.');
    if (dot == std::string::npos) {
      return 0;
    }
    try {
      return std::stoi(version_.substr(0, dot));
    } catch (...) {
      return 0;
    }
  }
}

bool PosixToolchainProvider::is_macos() const {
#ifdef __APPLE__
  return true;
#else
  return false;
#endif
}

bool PosixToolchainProvider::has(std::string_view cmd) const {
  return shell_->command_exists(std::string(cmd).c_str());
}

PosixToolchainProvider::PkgMgr PosixToolchainProvider::detect() const {
  if (is_macos()) {
    if (has("brew")) {
      return PkgMgr::Brew;
    }
    if (has("port")) {
      return PkgMgr::Port;
    }
    return PkgMgr::Unknown;
  }
  if (has("apt-get")) {
    return PkgMgr::Apt;
  }
  if (has("dnf")) {
    return PkgMgr::Dnf;
  }
  if (has("pacman")) {
    return PkgMgr::Pacman;
  }
  if (has("zypper")) {
    return PkgMgr::Zypper;
  }
  return PkgMgr::Unknown;
}

std::string PosixToolchainProvider::install_cmd(PkgMgr pm) const {
  switch (pm) {
    case PkgMgr::Apt:
      return fmt::format("sudo apt-get update && sudo apt-get install -y {}", pkg_name(pm));
    case PkgMgr::Dnf:
      return fmt::format("sudo dnf install -y {}", pkg_name(pm));
    case PkgMgr::Pacman:
      return fmt::format("sudo pacman -S --noconfirm {}", pkg_name(pm));
    case PkgMgr::Zypper:
      return fmt::format("sudo zypper install -y {}", pkg_name(pm));
    case PkgMgr::Brew:
      return fmt::format("brew install {}", pkg_name(pm));
    case PkgMgr::Port:
      return fmt::format("sudo port install {}", pkg_name(pm));
    default:
      return {};
  }
}

std::string PosixToolchainProvider::pkg_name(PkgMgr pm) const {
  // port uses 'clang'. Linux package names are uniformly 'gcc' or 'clang'.
  if (pm == PkgMgr::Brew) {
    return "llvm";
  }
  return compiler_;
}

std::string PosixToolchainProvider::bin_path(PkgMgr pm) const {
  switch (pm) {
    case PkgMgr::Brew:
      return fmt::format("/opt/homebrew/opt/{}/bin/{}", pkg_name(pm), compiler_);
    case PkgMgr::Port:
      return fmt::format("/opt/local/bin/{}", compiler_);
    default:
      return fmt::format("/usr/bin/{}", compiler_);
  }
}

}  // namespace sniffercommit::infrastructure
