#include <gtest/gtest.h>

#include <filesystem>
#include <string>

#include "metis/application/dependency_check_use_case.hpp"
#include "metis/domain/dependency.hpp"
#include "metis/domain/ports/file_system.hpp"
#include "metis/domain/ports/shell_executor.hpp"

namespace {

using namespace metis;

struct MockShell : domain::ports::IShellExecutor {
  std::string exec(const std::string& command) override { return command; }
  domain::ports::CapturedResult exec_captured(const std::string& command) override {
    return {.exit_code_ = 0, .output_ = command};
  }
  bool command_exists(const std::string& command) override {
    (void)command;
    return false;
  }
};

struct MockFs : domain::ports::IFileSystem {
  std::map<std::filesystem::path, std::string> files_;
  std::set<std::filesystem::path> dirs_;

  bool exists(const std::filesystem::path& path) override {
    return files_.contains(path) || dirs_.contains(path);
  }
  bool create_directories(const std::filesystem::path& path) override {
    (void)path;
    return true;
  }
  bool write_file(const std::filesystem::path& path, const std::string& content) override {
    files_[path] = content;
    return true;
  }
  std::string read_file(const std::filesystem::path& path) override {
    auto found = files_.find(path);
    return found != files_.end() ? found->second : "";
  }
  bool remove(const std::filesystem::path& path) override { return files_.erase(path) > 0; }
  bool set_permissions(const std::filesystem::path& path, std::filesystem::perms perms,
                       std::filesystem::perm_options options) override {
    (void)path;
    (void)perms;
    (void)options;
    return true;
  }
  std::filesystem::path current_path() override { return "/mock"; }
  std::filesystem::path absolute(const std::filesystem::path& path) override {
    return std::filesystem::absolute(path);
  }
};

TEST(DependencyCheckTest, EmptyRepoReturnsSuccess) {
  MockShell shell;
  MockFs mock_fs;
  application::DependencyCheckOptions opts;

  application::DependencyCheckUseCase use_case(std::make_unique<MockShell>(shell),
                                               std::make_unique<MockFs>(mock_fs));

  auto result = use_case.execute("/mock", opts);

  EXPECT_TRUE(result.success());
  EXPECT_TRUE(result.validations.empty());
}

TEST(DependencyCheckTest, ParsesConanRequires) {
  MockShell shell;
  MockFs mock_fs;
  mock_fs.files_["/mock/conanfile.py"] =
      R"(from conan import ConanFile
class Pkg(ConanFile):
    def requirements(self):
        self.requires("fmt/10.2.1")
        self.requires("spdlog/1.14.1")
)";

  application::DependencyCheckUseCase use_case(std::make_unique<MockShell>(shell),
                                               std::make_unique<MockFs>(mock_fs));

  auto result = use_case.execute("/mock", {});

  ASSERT_EQ(result.validations.size(), 2U);
  EXPECT_EQ(result.validations.at(0).dep.name, "fmt");
  EXPECT_EQ(result.validations.at(0).dep.version, "10.2.1");
  EXPECT_EQ(result.validations.at(0).dep.source, "conan");
  EXPECT_TRUE(result.validations.at(0).ok);
}

TEST(DependencyCheckTest, ParsesCMakeFetchContent) {
  MockShell shell;
  MockFs mock_fs;
  mock_fs.files_["/mock/CMakeLists.txt"] = R"(
FetchContent_Declare(fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    GIT_TAG 10.2.1
)
FetchContent_Declare(spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.14.1
)
)";

  application::DependencyCheckUseCase use_case(std::make_unique<MockShell>(shell),
                                               std::make_unique<MockFs>(mock_fs));

  auto result = use_case.execute("/mock", {});

  ASSERT_GE(result.validations.size(), 2U);
  EXPECT_EQ(result.validations.at(0).dep.name, "fmt");
  EXPECT_EQ(result.validations.at(0).dep.version, "10.2.1");
  EXPECT_EQ(result.validations.at(1).dep.name, "spdlog");
  EXPECT_EQ(result.validations.at(1).dep.version, "1.14.1");
}

TEST(DependencyCheckTest, InvalidSemverIsInvalid) {
  MockShell shell;
  MockFs mock_fs;
  mock_fs.files_["/mock/conanfile.py"] = R"(self.requires("fmt/not-a-version"))";

  application::DependencyCheckUseCase use_case(std::make_unique<MockShell>(shell),
                                               std::make_unique<MockFs>(mock_fs));

  auto result = use_case.execute("/mock", {});

  ASSERT_EQ(result.validations.size(), 1U);
  EXPECT_FALSE(result.validations.at(0).ok);
  EXPECT_NE(result.validations.at(0).message.find("invalid semver"), std::string::npos);
}

TEST(DependencyCheckTest, DetectsDuplicateAcrossSources) {
  MockShell shell;
  MockFs mock_fs;
  mock_fs.files_["/mock/conanfile.py"] = R"(self.requires("fmt/10.0.0"))";
  mock_fs.files_["/mock/CMakeLists.txt"] =
      R"(FetchContent_Declare(fmt VERSION 10.0.0 GIT_REPOSITORY https://example.com/fmt.git))";

  application::DependencyCheckUseCase use_case(std::make_unique<MockShell>(shell),
                                               std::make_unique<MockFs>(mock_fs));

  auto result = use_case.execute("/mock", {});

  EXPECT_FALSE(result.duplicates.empty());
}

TEST(DependencyCheckTest, MissingConanLockIsReported) {
  MockShell shell;
  MockFs mock_fs;
  mock_fs.files_["/mock/conanfile.py"] = R"(self.requires("fmt/1.0.0"))";

  application::DependencyCheckUseCase use_case(std::make_unique<MockShell>(shell),
                                               std::make_unique<MockFs>(mock_fs));

  auto result = use_case.execute("/mock", {});

  EXPECT_FALSE(result.lockfile_issues.empty());
  EXPECT_NE(result.lockfile_issues.at(0).find("conan.lock"), std::string::npos);
}

TEST(DependencyCheckTest, ParsesVcpkgManifest) {
  MockShell shell;
  MockFs mock_fs;
  mock_fs.files_["/mock/vcpkg.json"] = R"({
  "name": "demo",
  "version-string": "1.0.0",
  "dependencies": [
    { "name": "fmt", "version>=": "10.2.1" },
    { "name": "spdlog", "version>=": "1.14.1" }
  ]
})";

  application::DependencyCheckUseCase use_case(std::make_unique<MockShell>(shell),
                                               std::make_unique<MockFs>(mock_fs));

  auto result = use_case.execute("/mock", {});

  ASSERT_EQ(result.validations.size(), 2U);
  EXPECT_EQ(result.validations.at(0).dep.name, "fmt");
  EXPECT_EQ(result.validations.at(0).dep.version, "10.2.1");
  EXPECT_EQ(result.validations.at(0).dep.source, "vcpkg");
  EXPECT_TRUE(result.validations.at(0).ok);
}

TEST(DependencyCheckTest, ParsesVcpkgPlainStringDeps) {
  MockShell shell;
  MockFs mock_fs;
  mock_fs.files_["/mock/vcpkg.json"] = R"({ "dependencies": ["fmt", "spdlog"] })";

  application::DependencyCheckUseCase use_case(std::make_unique<MockShell>(shell),
                                               std::make_unique<MockFs>(mock_fs));

  auto result = use_case.execute("/mock", {});

  ASSERT_EQ(result.validations.size(), 2U);
  EXPECT_EQ(result.validations.at(0).dep.name, "fmt");
  EXPECT_EQ(result.validations.at(1).dep.name, "spdlog");
}

TEST(DependencyCheckTest, MissingVcpkgLockfileIsReported) {
  MockShell shell;
  MockFs mock_fs;
  mock_fs.files_["/mock/vcpkg.json"] = R"({ "dependencies": ["fmt"] })";

  application::DependencyCheckUseCase use_case(std::make_unique<MockShell>(shell),
                                               std::make_unique<MockFs>(mock_fs));

  auto result = use_case.execute("/mock", {});

  EXPECT_FALSE(result.lockfile_issues.empty());
  EXPECT_NE(result.lockfile_issues.at(0).find("vcpkg-configuration.json"), std::string::npos);
}

}  // namespace
