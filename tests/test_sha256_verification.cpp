#include <gtest/gtest.h>

#include <filesystem>
#include <functional>
#include <string>

#include "metis/domain/ports/file_system.hpp"
#include "metis/domain/ports/shell_executor.hpp"
#include "metis/infrastructure/windows_gcc_provider.hpp"

namespace {

using namespace metis;

// Real SHA-256 of winlibs-x86_64-posix-seh-gcc-14.2.0-llvm-19.1.1-mingw-w64ucrt-12.0.0-r2.zip.
constexpr char kRealHash[] = "12fa72d2566e641c3bf0213a946d33d8bef2e0757af2fb3ed60a995e05d74606";

struct MockShell : domain::ports::IShellExecutor {
  std::function<domain::ports::CapturedResult(const std::string&)> handler;

  std::string exec(const std::string&) override { return {}; }
  domain::ports::CapturedResult exec_captured(const std::string& cmd) override {
    return handler(cmd);
  }
  bool command_exists(const std::string&) override { return true; }
};

struct MockFileSystem : domain::ports::IFileSystem {
  bool exists(const std::filesystem::path&) override { return true; }
  bool create_directories(const std::filesystem::path&) override { return true; }
  bool write_file(const std::filesystem::path&, const std::string&) override { return true; }
  std::string read_file(const std::filesystem::path&) override { return {}; }
  bool set_permissions(const std::filesystem::path&, std::filesystem::perms,
                       std::filesystem::perm_options) override {
    return true;
  }
  std::filesystem::path current_path() override { return "/mock"; }
  std::filesystem::path absolute(const std::filesystem::path& path) override {
    return std::filesystem::absolute(path);
  }
};

domain::ports::CapturedResult ok(std::string output) {
  return {.exit_code_ = 0, .output_ = std::move(output)};
}
domain::ports::CapturedResult fail() { return {.exit_code_ = 1, .output_ = {}}; }

std::string certutil_output() {
  return "SHA256 hash of C:\\fake\\archive.zip:\n" + std::string(kRealHash) +
         "\nCertUtil: -hashfile command completed successfully.\n";
}

TEST(Sha256VerificationTest, AcceptsMatchingChecksum) {
  MockShell shell;
  shell.handler = [](const std::string& cmd) -> domain::ports::CapturedResult {
    if (cmd.starts_with("curl")) {
      return ok(std::string(kRealHash) + " winlibs-x86_64-posix-seh-gcc-14.2.0-r2.zip");
    }
    if (cmd.starts_with("certutil")) {
      return ok(certutil_output());
    }
    if (cmd.starts_with("gcc")) {
      return ok("gcc (MinGW-W64) 14.2.0\n");
    }
    return fail();
  };
  MockFileSystem fs;
  auto provider = infrastructure::WindowsGccProvider(&shell, &fs, "14.2.0");

  auto result = provider.install("C:\\fake\\archive.zip");

  EXPECT_TRUE(result.success_);
  EXPECT_TRUE(result.error_message_.empty());
}

TEST(Sha256VerificationTest, RejectsMismatchedChecksum) {
  MockShell shell;
  shell.handler = [](const std::string& cmd) -> domain::ports::CapturedResult {
    if (cmd.starts_with("curl")) {
      return ok(std::string(64, '0') + " winlibs.zip");
    }
    if (cmd.starts_with("certutil")) {
      return ok(certutil_output());
    }
    return fail();
  };
  MockFileSystem fs;
  auto provider = infrastructure::WindowsGccProvider(&shell, &fs, "14.2.0");

  auto result = provider.install("C:\\fake\\archive.zip");

  EXPECT_FALSE(result.success_);
  EXPECT_NE(result.error_message_.find("SHA-256 verification failed"), std::string::npos);
}

TEST(Sha256VerificationTest, FailsClosedWhenChecksumUnavailable) {
  MockShell shell;
  shell.handler = [](const std::string&) -> domain::ports::CapturedResult { return fail(); };
  MockFileSystem fs;
  auto provider = infrastructure::WindowsGccProvider(&shell, &fs, "14.2.0");

  auto result = provider.install("C:\\fake\\archive.zip");

  EXPECT_FALSE(result.success_);
  EXPECT_NE(result.error_message_.find("Refusing to install"), std::string::npos);
}

}  // namespace
