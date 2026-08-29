#include <gtest/gtest.h>

#include <filesystem>

#include "metis/application/install_toolchain_use_case.hpp"
#include "metis/domain/ports/archive_extractor.hpp"
#include "metis/domain/ports/file_system.hpp"
#include "metis/domain/ports/http_client.hpp"
#include "metis/domain/ports/toolchain_provider.hpp"

namespace {

using namespace metis;

struct MockProvider : domain::ports::IToolchainProvider {
  bool installed_ = false;
  std::string ver_ = "test-version";
  std::string desc_ = "mock-provider";
  std::string dl_url_;

  [[nodiscard]] bool is_installed() const override { return installed_; }
  [[nodiscard]] std::optional<std::string> get_version() const override { return ver_; }
  [[nodiscard]] domain::ports::ToolchainPackage resolve_package() const override {
    return {.name_ = "gcc",
            .version_ = "12.3.0",
            .download_url_ = dl_url_,
            .checksum_ = "",
            .archive_type_ = "",
            .install_dir_ = "/mock"};
  }
  [[nodiscard]] domain::ports::ToolchainInstallResult install(
      const std::filesystem::path& /*archive_path*/) override {
    installed_ = true;
    return {.success_ = true,
            .installed_path_ = "/mock/gcc",
            .version_ = "12.3.0",
            .error_message_ = ""};
  }
  [[nodiscard]] bool supports_cpp_standard(domain::ports::CppStandard /*standard*/) const override {
    return true;
  }
  [[nodiscard]] domain::ports::CppStandard max_supported_standard() const override {
    return domain::ports::CppStandard::CPP_23;
  }
  [[nodiscard]] std::string description() const override { return desc_; }
};

struct MockHttpClient : domain::ports::IHttpClient {
  bool fail_download_ = false;

  [[nodiscard]] domain::ports::DownloadResult download(const std::string& /*url*/,
                                                       const std::filesystem::path& /*dest_dir*/,
                                                       const std::string& /*filename*/) override {
    if (fail_download_) {
      return {.success_ = false, .download_path_ = "", .error_message_ = "mock failure"};
    }
    return {.success_ = true, .download_path_ = "/mock/archive.tar.gz", .error_message_ = ""};
  }
};

struct MockArchiveExtractor : domain::ports::IArchiveExtractor {
  [[nodiscard]] domain::ports::ExtractionResult extract(
      const std::filesystem::path& /*archive_path*/,
      const std::filesystem::path& /*dest_dir*/) override {
    return {.success_ = true, .extracted_root_ = "/mock/extracted", .error_message_ = ""};
  }
};

struct MockFileSystem : domain::ports::IFileSystem {
  bool exists(const std::filesystem::path& /*path*/) override { return false; }
  bool create_directories(const std::filesystem::path& /*path*/) override { return true; }
  bool write_file(const std::filesystem::path& /*path*/, const std::string& /*content*/) override {
    return true;
  }
  std::string read_file(const std::filesystem::path& /*path*/) override { return {}; }
  bool set_permissions(const std::filesystem::path& /*path*/, std::filesystem::perms /*perms*/,
                       std::filesystem::perm_options /*opts*/) override {
    return true;
  }
  std::filesystem::path current_path() override { return "/mock"; }
  std::filesystem::path absolute(const std::filesystem::path& path) override {
    return std::filesystem::absolute(path);
  }
};

application::InstallToolchainUseCase make_use_case(
    std::unique_ptr<domain::ports::IToolchainProvider> provider,
    std::unique_ptr<domain::ports::IHttpClient> http,
    std::unique_ptr<domain::ports::IArchiveExtractor> archive_extractor,
    std::unique_ptr<domain::ports::IFileSystem> file_system) {
  return {std::move(provider), std::move(http), std::move(archive_extractor),
          std::move(file_system)};
}

TEST(ToolchainInstallTest, AlreadyInstalledReturnsEarly) {
  auto provider = std::make_unique<MockProvider>();
  provider->installed_ = true;

  auto use_case =
      make_use_case(std::move(provider), std::make_unique<MockHttpClient>(),
                    std::make_unique<MockArchiveExtractor>(), std::make_unique<MockFileSystem>());

  application::InstallToolchainOptions opts;
  auto result = use_case.execute(opts);

  EXPECT_TRUE(result.was_already_installed_);
  EXPECT_EQ(result.version_, "test-version");
}

TEST(ToolchainInstallTest, ForceReinstalls) {
  auto provider = std::make_unique<MockProvider>();
  provider->installed_ = true;

  auto use_case =
      make_use_case(std::move(provider), std::make_unique<MockHttpClient>(),
                    std::make_unique<MockArchiveExtractor>(), std::make_unique<MockFileSystem>());

  application::InstallToolchainOptions opts;
  opts.force_ = true;
  auto result = use_case.execute(opts);

  EXPECT_TRUE(result.success_);
  EXPECT_FALSE(result.was_already_installed_);
}

TEST(ToolchainInstallTest, DryRunSkipsInstall) {
  auto use_case =
      make_use_case(std::make_unique<MockProvider>(), std::make_unique<MockHttpClient>(),
                    std::make_unique<MockArchiveExtractor>(), std::make_unique<MockFileSystem>());

  application::InstallToolchainOptions opts;
  opts.dry_run_ = true;
  auto result = use_case.execute(opts);

  EXPECT_TRUE(result.success_);
  EXPECT_TRUE(result.error_message_.find("[DRY-RUN]") != std::string::npos);
}

TEST(ToolchainInstallTest, DownloadFailureReturnsError) {
  auto http = std::make_unique<MockHttpClient>();
  http->fail_download_ = true;

  auto provider = std::make_unique<MockProvider>();
  provider->dl_url_ = "https://example.com/gcc.tar.gz";

  auto use_case =
      make_use_case(std::move(provider), std::move(http), std::make_unique<MockArchiveExtractor>(),
                    std::make_unique<MockFileSystem>());

  application::InstallToolchainOptions opts;
  auto result = use_case.execute(opts);

  EXPECT_FALSE(result.success_);
  EXPECT_TRUE(result.error_message_.find("Download failed") != std::string::npos);
}

TEST(ToolchainInstallTest, SuccessfulInstallFlow) {
  auto provider = std::make_unique<MockProvider>();

  auto use_case =
      make_use_case(std::move(provider), std::make_unique<MockHttpClient>(),
                    std::make_unique<MockArchiveExtractor>(), std::make_unique<MockFileSystem>());

  application::InstallToolchainOptions opts;
  opts.install_prefix_ = "/mock/prefix";
  auto result = use_case.execute(opts);

  EXPECT_TRUE(result.success_);
  EXPECT_EQ(result.installed_path_, "/mock/gcc");
  EXPECT_EQ(result.version_, "12.3.0");
}

}  // namespace
