#include <gtest/gtest.h>

#include <filesystem>
#include <string>

#include "sniffercommit/application/dependency_check_use_case.hpp"
#include "sniffercommit/domain/dependency.hpp"
#include "sniffercommit/domain/ports/file_system.hpp"
#include "sniffercommit/domain/ports/shell_executor.hpp"

namespace {

using namespace sniffercommit;

struct MockShell : domain::ports::IShellExecutor {
  std::string exec(const std::string&) override { return {}; }
  domain::ports::CapturedResult exec_captured(const std::string&) override {
    return {0, ""};
  }
  bool command_exists(const std::string&) override { return false; }
};

struct MockFs : domain::ports::IFileSystem {
  std::map<std::filesystem::path, std::string> files;
  std::set<std::filesystem::path> dirs;

  bool exists(const std::filesystem::path& p) override {
    return files.count(p) > 0 || dirs.count(p) > 0;
  }
  bool create_directories(const std::filesystem::path&) override { return true; }
  bool write_file(const std::filesystem::path& p, const std::string& content) override {
    files[p] = content;
    return true;
  }
  std::string read_file(const std::filesystem::path& p) override {
    auto it = files.find(p);
    return it != files.end() ? it->second : "";
  }
  bool remove(const std::filesystem::path& p) override { return files.erase(p) > 0; }
  bool set_permissions(const std::filesystem::path&, std::filesystem::perms,
                       std::filesystem::perm_options) override {
    return true;
  }
  std::filesystem::path current_path() override { return "/mock"; }
  std::filesystem::path absolute(const std::filesystem::path& p) override {
    return std::filesystem::absolute(p);
  }
};

TEST(DependencyCheckTest, EmptyRepoReturnsSuccess) {
  MockShell shell;
  MockFs fs;
  application::DependencyCheckOptions opts;

  application::DependencyCheckUseCase use_case(
      std::make_unique<MockShell>(shell), std::make_unique<MockFs>(fs));

  auto result = use_case.execute("/mock", opts);

  EXPECT_TRUE(result.success());
  EXPECT_TRUE(result.validations.empty());
}

TEST(DependencyCheckTest, ParsesConanRequires) {
  MockShell shell;
  MockFs fs;
  fs.files["/mock/conanfile.py"] =
      R"(from conan import ConanFile
class Pkg(ConanFile):
    def requirements(self):
        self.requires("fmt/10.2.1")
        self.requires("spdlog/1.14.1")
)";

  application::DependencyCheckUseCase use_case(
      std::make_unique<MockShell>(shell), std::make_unique<MockFs>(fs));

  auto result = use_case.execute("/mock", {});

  ASSERT_EQ(result.validations.size(), 2u);
  EXPECT_EQ(result.validations[0].dep.name, "fmt");
  EXPECT_EQ(result.validations[0].dep.version, "10.2.1");
  EXPECT_EQ(result.validations[0].dep.source, "conan");
  EXPECT_TRUE(result.validations[0].ok);
}

TEST(DependencyCheckTest, ParsesCMakeFetchContent) {
  MockShell shell;
  MockFs fs;
  fs.files["/mock/CMakeLists.txt"] = R"(
FetchContent_Declare(fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    GIT_TAG 10.2.1
)
FetchContent_Declare(spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.14.1
)
)";

  application::DependencyCheckUseCase use_case(
      std::make_unique<MockShell>(shell), std::make_unique<MockFs>(fs));

  auto result = use_case.execute("/mock", {});

  ASSERT_GE(result.validations.size(), 2u);
  EXPECT_EQ(result.validations[0].dep.name, "fmt");
  EXPECT_EQ(result.validations[0].dep.version, "10.2.1");
  EXPECT_EQ(result.validations[1].dep.name, "spdlog");
  EXPECT_EQ(result.validations[1].dep.version, "1.14.1");
}

TEST(DependencyCheckTest, InvalidSemverIsInvalid) {
  MockShell shell;
  MockFs fs;
  fs.files["/mock/conanfile.py"] = R"(self.requires("fmt/not-a-version"))";

  application::DependencyCheckUseCase use_case(
      std::make_unique<MockShell>(shell), std::make_unique<MockFs>(fs));

  auto result = use_case.execute("/mock", {});

  ASSERT_EQ(result.validations.size(), 1u);
  EXPECT_FALSE(result.validations[0].ok);
  EXPECT_NE(result.validations[0].message.find("invalid semver"), std::string::npos);
}

TEST(DependencyCheckTest, DetectsDuplicateAcrossSources) {
  MockShell shell;
  MockFs fs;
  fs.files["/mock/conanfile.py"] = R"(self.requires("fmt/10.0.0"))";
  fs.files["/mock/CMakeLists.txt"] =
      R"(FetchContent_Declare(fmt VERSION 10.0.0 GIT_REPOSITORY https://example.com/fmt.git))";

  application::DependencyCheckUseCase use_case(
      std::make_unique<MockShell>(shell), std::make_unique<MockFs>(fs));

  auto result = use_case.execute("/mock", {});

  EXPECT_FALSE(result.duplicates.empty());
}

TEST(DependencyCheckTest, MissingConanLockIsReported) {
  MockShell shell;
  MockFs fs;
  fs.files["/mock/conanfile.py"] = R"(self.requires("fmt/1.0.0"))";

  application::DependencyCheckUseCase use_case(
      std::make_unique<MockShell>(shell), std::make_unique<MockFs>(fs));

  auto result = use_case.execute("/mock", {});

  EXPECT_FALSE(result.lockfile_issues.empty());
  EXPECT_NE(result.lockfile_issues[0].find("conan.lock"), std::string::npos);
}

}  // namespace
