#include <gtest/gtest.h>

#include <filesystem>
#include <string>

#include "metis/application/sanitizer_checks_use_case.hpp"
#include "metis/domain/config.hpp"
#include "metis/domain/ports/file_system.hpp"
#include "metis/domain/ports/shell_executor.hpp"

namespace {

using namespace metis;

struct MockShell : domain::ports::IShellExecutor {
  std::string exec(const std::string&) override { return {}; }
  domain::ports::CapturedResult exec_captured(const std::string&) override { return {0, ""}; }
  bool command_exists(const std::string&) override { return true; }
};

struct MockFs : domain::ports::IFileSystem {
  bool dir_exists = true;
  bool exists(const std::filesystem::path&) override { return dir_exists; }
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

TEST(SanitizerChecksTest, EmptyTypesReturnsTrue) {
  MockShell shell;
  MockFs fs;
  domain::config::ProjectConfig cfg;

  application::SanitizerChecksUseCase use_case(std::make_unique<MockShell>(shell),
                                               std::make_unique<MockFs>(fs));

  EXPECT_TRUE(use_case.execute(cfg, "/mock", false));
}

TEST(SanitizerChecksTest, UnknownTypeReturnsFalse) {
  MockShell shell;
  MockFs fs;
  domain::config::ProjectConfig cfg;
  cfg.sanitizer.types = {"bogus"};

  application::SanitizerChecksUseCase use_case(std::make_unique<MockShell>(shell),
                                               std::make_unique<MockFs>(fs));

  EXPECT_FALSE(use_case.execute(cfg, "/mock", false));
}

TEST(SanitizerChecksTest, BuildDirMissingReturnsFalse) {
  MockShell shell;
  MockFs fs;
  fs.dir_exists = false;
  domain::config::ProjectConfig cfg;
  cfg.sanitizer.types = {"address"};

  application::SanitizerChecksUseCase use_case(std::make_unique<MockShell>(shell),
                                               std::make_unique<MockFs>(fs));

  EXPECT_FALSE(use_case.execute(cfg, "/mock", false));
}

}  // namespace
