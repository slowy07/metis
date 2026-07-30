#ifndef SNIFFERCOMMIT_APPLICATION_INSTALL_TOOLCHAIN_USE_CASE_HPP
#define SNIFFERCOMMIT_APPLICATION_INSTALL_TOOLCHAIN_USE_CASE_HPP

#include <filesystem>
#include <memory>
#include <string>

#include "sniffercommit/domain/ports/archive_extractor.hpp"
#include "sniffercommit/domain/ports/file_system.hpp"
#include "sniffercommit/domain/ports/http_client.hpp"
#include "sniffercommit/domain/ports/toolchain_provider.hpp"

namespace sniffercommit::application {

struct InstallToolchainOptions {
  std::string compiler_ = "gcc";
  std::string version_;
  std::filesystem::path install_prefix_;
  bool force_ = false;
  bool dry_run_ = false;
};

struct InstallToolchainResult {
  bool success_ = false;
  bool was_already_installed_ = false;
  std::string installed_path_;
  std::string version_;
  std::string error_message_;
};

class InstallToolchainUseCase {
 public:
  InstallToolchainUseCase(std::unique_ptr<domain::ports::IToolchainProvider> provider,
                          std::unique_ptr<domain::ports::IHttpClient> http_client,
                          std::unique_ptr<domain::ports::IArchiveExtractor> extractor,
                          std::unique_ptr<domain::ports::IFileSystem> file_system);

  [[nodiscard]] InstallToolchainResult execute(const InstallToolchainOptions& opts);

 private:
  std::unique_ptr<domain::ports::IToolchainProvider> provider_;
  std::unique_ptr<domain::ports::IHttpClient> http_client_;
  std::unique_ptr<domain::ports::IArchiveExtractor> extractor_;
  std::unique_ptr<domain::ports::IFileSystem> file_system_;
};

}  // namespace sniffercommit::application

#endif  // !SNIFFERCOMMIT_APPLICATION_INSTALL_TOOLCHAIN_USE_CASE_HPP
