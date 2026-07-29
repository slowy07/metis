#include <gtest/gtest.h>

#include <filesystem>

#include "sniffercommit/application/install_toolchain_use_case.hpp"
#include "sniffercommit/domain/ports/archive_extractor.hpp"
#include "sniffercommit/domain/ports/file_system.hpp"
#include "sniffercommit/domain/ports/http_client.hpp"
#include "sniffercommit/domain/ports/toolchain_provider.hpp"

namespace {

using namespace sniffercommit;

struct MockProvider : domain::ports::IToolchainProvider {
  bool installed = false;
  std::string ver = "test-version";
  std::string desc = "mock-provider";
  std::string dl_url;

  bool is_installed() const override { return installed; }
  std::optional<std::string> get_version() const override { return ver; }
  domain::ports::ToolchainPackage resolve_package() const override {
    return {"gcc", "12.3.0", dl_url, "", "", "/mock"};
  }
  domain::ports::ToolchainInstallResult install(
      const std::filesystem::path&) override {
    installed = true;
    return {.success_ = true, .installed_path_ = "/mock/gcc", .version_ = "12.3.0"};
  }
  std::string description() const override { return desc; }
};

struct MockHttpClient : domain::ports::IHttpClient {
  bool fail_download = false;
  domain::ports::DownloadResult download(const std::string&,
                                          const std::filesystem::path&,
                                          const std::string&) override {
    if (fail_download) return {.success_ = false, .error_message_ = "mock failure"};
    return {.success_ = true, .download_path_ = "/mock/archive.tar.gz"};
  }
};

struct MockArchiveExtractor : domain::ports::IArchiveExtractor {
  domain::ports::ExtractionResult extract(const std::filesystem::path&,
                                           const std::filesystem::path&) override {
    return {.success_ = true, .extracted_root_ = "/mock/extracted"};
  }
};

struct MockFileSystem : domain::ports::IFileSystem {
  bool exists(const std::filesystem::path&) override { return false; }
  bool create_directories(const std::filesystem::path&) override { return true; }
  bool write_file(const std::filesystem::path&, const std::string&) override { return true; }
  std::string read_file(const std::filesystem::path&) override { return {}; }
  bool remove(const std::filesystem::path&) override { return true; }
  bool set_permissions(const std::filesystem::path&, std::filesystem::perms,
                       std::filesystem::perm_options) override { return true; }
  std::filesystem::path current_path() override { return "/mock"; }
  std::filesystem::path absolute(const std::filesystem::path& p) override {
    return std::filesystem::absolute(p);
  }
};

application::InstallToolchainUseCase make_use_case(
    std::unique_ptr<domain::ports::IToolchainProvider> provider,
    std::unique_ptr<domain::ports::IHttpClient> http,
    std::unique_ptr<domain::ports::IArchiveExtractor> ar,
    std::unique_ptr<domain::ports::IFileSystem> fs) {
  return application::InstallToolchainUseCase(
      std::move(provider), std::move(http), std::move(ar), std::move(fs));
}

TEST(ToolchainInstallTest, AlreadyInstalledReturnsEarly) {
  auto provider = std::make_unique<MockProvider>();
  provider->installed = true;

  auto uc = make_use_case(
      std::move(provider),
      std::make_unique<MockHttpClient>(),
      std::make_unique<MockArchiveExtractor>(),
      std::make_unique<MockFileSystem>());

  application::InstallToolchainOptions opts;
  auto result = uc.execute(opts);

  EXPECT_TRUE(result.was_already_installed_);
  EXPECT_EQ(result.version_, "test-version");
}

TEST(ToolchainInstallTest, ForceReinstalls) {
  auto provider = std::make_unique<MockProvider>();
  provider->installed = true;

  auto uc = make_use_case(
      std::move(provider),
      std::make_unique<MockHttpClient>(),
      std::make_unique<MockArchiveExtractor>(),
      std::make_unique<MockFileSystem>());

  application::InstallToolchainOptions opts;
  opts.force_ = true;
  auto result = uc.execute(opts);

  EXPECT_TRUE(result.success_);
  EXPECT_FALSE(result.was_already_installed_);
}

TEST(ToolchainInstallTest, DryRunSkipsInstall) {
  auto uc = make_use_case(
      std::make_unique<MockProvider>(),
      std::make_unique<MockHttpClient>(),
      std::make_unique<MockArchiveExtractor>(),
      std::make_unique<MockFileSystem>());

  application::InstallToolchainOptions opts;
  opts.dry_run_ = true;
  auto result = uc.execute(opts);

  EXPECT_TRUE(result.success_);
  EXPECT_TRUE(result.error_message_.find("[DRY-RUN]") != std::string::npos);
}

TEST(ToolchainInstallTest, DownloadFailureReturnsError) {
  auto http = std::make_unique<MockHttpClient>();
  http->fail_download = true;

  auto provider = std::make_unique<MockProvider>();
  provider->dl_url = "https://example.com/gcc.tar.gz";

  auto uc = make_use_case(
      std::move(provider),
      std::move(http),
      std::make_unique<MockArchiveExtractor>(),
      std::make_unique<MockFileSystem>());

  application::InstallToolchainOptions opts;
  auto result = uc.execute(opts);

  EXPECT_FALSE(result.success_);
  EXPECT_TRUE(result.error_message_.find("Download failed") != std::string::npos);
}

TEST(ToolchainInstallTest, SuccessfulInstallFlow) {
  auto provider = std::make_unique<MockProvider>();

  auto uc = make_use_case(
      std::move(provider),
      std::make_unique<MockHttpClient>(),
      std::make_unique<MockArchiveExtractor>(),
      std::make_unique<MockFileSystem>());

  application::InstallToolchainOptions opts;
  opts.install_prefix_ = "/mock/prefix";
  auto result = uc.execute(opts);

  EXPECT_TRUE(result.success_);
  EXPECT_EQ(result.installed_path_, "/mock/gcc");
  EXPECT_EQ(result.version_, "12.3.0");
}

}  // namespace
