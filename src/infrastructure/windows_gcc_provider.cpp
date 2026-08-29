#include "metis/infrastructure/windows_gcc_provider.hpp"

#include <fmt/format.h>

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <ranges>
#include <string>

#include "metis/domain/ports/file_system.hpp"
#include "metis/domain/ports/shell_executor.hpp"
#include "metis/domain/ports/toolchain_provider.hpp"

namespace metis::infrastructure {

namespace {
std::optional<std::string> parse_sha256_token(const std::string& raw) {
  // A SHA-256 hex digest is exactly 64 hex chars, optionally followed by a
  // filename/whitespace. Accept only that shape.
  auto end = raw.find_first_not_of("0123456789abcdefABCDEF");

  if (end == std::string::npos) {
    end = raw.size();
  }

  if (end != 64) {
    return std::nullopt;
  }

  return raw.substr(0, end);
}

std::optional<std::string> sha256_via_cerutil(domain::ports::IShellExecutor* shell,
                                              const std::filesystem::path& path) {
  auto res = shell->exec_captured(fmt::format(R"(certutil -hashfile "{}" SHA256)", path.string()));

  if (res.exit_code_ != 0 || res.output_.empty()) {
    return std::nullopt;
  }

  std::istringstream iss(res.output_);
  std::string header;
  std::string hash_line;

  if (!std::getline(iss, header)) {
    return std::nullopt;
  }

  if (!std::getline(iss, hash_line)) {
    return std::nullopt;
  }

  auto [first, last] =
      std::ranges::remove_if(hash_line, [](unsigned char chr) { return std::isspace(chr); });
  hash_line.erase(first, last);

  return hash_line;
}

std::optional<std::string> fetch_expected_sha256(domain::ports::IShellExecutor* shell,
                                                 const std::string& sha256_url) {
  auto curl = shell->exec_captured(fmt::format(R"(curl -sLf "{}")", sha256_url));

  if (curl.exit_code_ == 0 && !curl.output_.empty()) {
    return parse_sha256_token(curl.output_);
  }

  auto pshell = shell->exec_captured(
      fmt::format(R"(powershell -Command "(Invoke-WebRequest -Uri '{}' -UseBasicParsing).Content")",
                  sha256_url));

  if (pshell.exit_code_ == 0 && !pshell.output_.empty()) {
    return parse_sha256_token(pshell.output_);
  }

  return std::nullopt;
}
}  // namespace

WindowsGccProvider::WindowsGccProvider(domain::ports::IShellExecutor* shell,
                                       domain::ports::IFileSystem* fs, std::string version)
  : shell_(shell)
  , fs_(fs)
  , version_(version.empty() ? "14.2.0" : version)
  , install_prefix_(default_install_prefix()) {}

bool WindowsGccProvider::is_installed() const {
  return shell_->command_exists("gcc") || shell_->command_exists("gcc.exe");
}

std::optional<std::string> WindowsGccProvider::get_version() const {
  if (!is_installed()) {
    return std::nullopt;
  }

  auto result = shell_->exec_captured("gcc --version");
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

  auto sha256_url = build_download_url() + ".sha256";
  auto expected = fetch_expected_sha256(shell_, sha256_url);

  if (!expected) {
    result.error_message_ = fmt::format(
        "Could not fetch expected SHA-256 checksum from {}.\n"
        "Refusing to install without integrity verification.",
        sha256_url);
    return result;
  }

  auto actual = sha256_via_cerutil(shell_, archive_path);

  if (!actual) {
    result.error_message_ = "Failed to compute SHA-256 of download archive via certutil";
    return result;
  }

  if (expected->size() != actual->size() ||
      !std::equal(expected->begin(), expected->end(), actual->begin(),
                  [](unsigned char chra, unsigned char chrb) {
                    return std::tolower(chra) == std::tolower(chrb);
                  })) {
    result.error_message_ = fmt::format(
        "SHA-256 verification failed.\n"
        "Expected: {}\n"
        "Actual:   {}\n"
        "The archive may be corrupted or tampered with. Delete the file and retry.",
        *expected, *actual);
    return result;
  }

  if (!fs_->create_directories(install_prefix_)) {
    result.error_message_ = "Failed to create installation directory";
    return result;
  }

  auto gcc_bin = install_prefix_ / "mingw64" / "bin" / "gcc.exe";
  if (!fs_->exists(gcc_bin)) {
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
    return std::filesystem::path("C:\\") / "metis" / "toolchains" /
           fmt::format("mingw-{}", version_);
  }
  return std::filesystem::path(home) / ".metis" / "toolchains" / fmt::format("mingw-{}", version_);
}

std::string WindowsGccProvider::build_download_url() const {
  return fmt::format(
      "https://github.com/brechtsanders/winlibs_mingw/releases/download/"
      "{}posix-19.1.1-12.0.0-ucrt-r2/"
      "winlibs-x86_64-posix-seh-gcc-{}-llvm-19.1.1-mingw-w64ucrt-12.0.0-r2.zip",
      version_, version_);
}

bool WindowsGccProvider::supports_cpp_standard(domain::ports::CppStandard standard) const {
  return static_cast<int>(standard) <= static_cast<int>(max_supported_standard());
}

domain::ports::CppStandard WindowsGccProvider::max_supported_standard() const {
  auto major = parse_major_version();
  if (major == 0) {
    return domain::ports::CppStandard::CPP_23;
  }
  if (major == 15) {
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

int WindowsGccProvider::parse_major_version() const {
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

}  // namespace metis::infrastructure
