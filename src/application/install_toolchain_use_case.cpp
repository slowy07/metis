#include "metis/application/install_toolchain_use_case.hpp"

#include <fmt/format.h>

#include <filesystem>

namespace metis::application {
InstallToolchainUseCase::InstallToolchainUseCase(
    std::unique_ptr<domain::ports::IToolchainProvider> provider,
    std::unique_ptr<domain::ports::IHttpClient> http_client,
    std::unique_ptr<domain::ports::IArchiveExtractor> extractor,
    std::unique_ptr<domain::ports::IFileSystem> file_system)
  : provider_(std::move(provider))
  , http_client_(std::move(http_client))
  , extractor_(std::move(extractor))
  , file_system_(std::move(file_system)) {}

InstallToolchainResult InstallToolchainUseCase::execute(const InstallToolchainOptions& opts) {
  InstallToolchainResult result;

  if (!provider_->supports_cpp_standard(opts.cpp_standard_)) {
    result.error_message_ = fmt::format(
        "The requested C++{} standard is not supported by this compiler/version. "
        "Maximum supported: C++{}.",
        static_cast<int>(opts.cpp_standard_),
        static_cast<int>(provider_->max_supported_standard()));
    return result;
  }

  if (provider_->is_installed() && !opts.force_) {
    auto ver = provider_->get_version();
    result.success_ = true;
    result.was_already_installed_ = true;
    result.version_ = ver.value_or("unknown");
    result.error_message_ =
        fmt::format("{} is already installed (version: {}), use --force to reinstall",
                    provider_->description(), result.version_);

    return result;
  }

  auto package = provider_->resolve_package();

  std::filesystem::path install_prefix = opts.install_prefix_;
  if (install_prefix.empty()) {
    const char* home = std::getenv("HOME");
#ifdef WIN32
    if (home == nullptr) {
      home = std::getenv("USERPROFILE");
    }
#endif  // WIN32
    if (home == nullptr) {
      result.error_message_ = "Cannot determine home directory. set HOME or use --prefix";
      return result;
    }

    install_prefix = std::filesystem::path(home) / ".local" / "metis" / "toolchain" /
                     fmt::format("{}.{}", package.name_, package.version_);
  }

  if (!file_system_->create_directories(install_prefix)) {
    result.error_message_ = fmt::format("Failed to create directory: {}", install_prefix.string());
    return result;
  }

  if (opts.dry_run_) {
    result.success_ = true;
    result.installed_path_ = install_prefix.string();
    result.version_ = package.version_;
    result.error_message_ =
        fmt::format("[DRY-RUN] Would install {} {} to {}", provider_->description(),
                    package.version_, install_prefix.string());

    return result;
  }

  std::filesystem::path archive_path;
  if (!package.download_url_.empty()) {
    auto dl_result = http_client_->download(package.download_url_, install_prefix, {});

    if (!dl_result.success_) {
      result.error_message_ = fmt::format("Download failed: {}", dl_result.error_message_);
      return result;
    }

    archive_path = dl_result.download_path_;
  }

  auto install_result = provider_->install(archive_path);
  if (!install_result.success_) {
    result.error_message_ = install_result.error_message_;
    return result;
  }

  if (!provider_->is_installed()) {
    result.error_message_ =
        "Installation completed but compiler is not in PATH ."
        "need to restart the SHELL or add on bin directory on PATH";
    return result;
  }

  result.success_ = true;
  result.installed_path_ = install_result.installed_path_;
  result.version_ = install_result.version_;

  return result;
}

}  // namespace metis::application
