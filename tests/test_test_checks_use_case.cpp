#include <gtest/gtest.h>

#include <filesystem>
#include <string>

#include "metis/application/test_checks_use_case.hpp"
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
  bool remove(const std::filesystem::path&) override { return true; }
  bool set_permissions(const std::filesystem::path&, std::filesystem::perms,
                       std::filesystem::perm_options) override {
    return true;
  }
  std::filesystem::path current_path() override { return "/mock"; }
  std::filesystem::path absolute(const std::filesystem::path& path) override {
    return std::filesystem::absolute(path);
  }
};

TEST(TestChecksUseCaseTest, MissingBuildDirReturnsError) {
  MockShell shell;
  MockFs fs;
  fs.dir_exists = false;
  domain::config::ProjectConfig cfg;
  cfg.test.build_dir = "build";

  application::TestChecksUseCase use_case(std::make_unique<MockShell>(shell),
                                          std::make_unique<MockFs>(fs));

  auto result = use_case.execute(cfg, "/mock", false, false);

  EXPECT_FALSE(result.success);
  EXPECT_NE(result.output.find("does not exists"), std::string::npos);
}

}  // namespace
