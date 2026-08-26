#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <memory>
#include <typeinfo>

#include "metis/application/checks/clang_tidy_check.hpp"
#include "metis/application/checks/compiler_check.hpp"
#include "metis/application/checks/shell_check.hpp"
#include "metis/application/run_checks_use_case.hpp"
#include "metis/domain/config.hpp"
#include "metis/infrastructure/os_file_system.hpp"
#include "metis/infrastructure/process_shell_executor.hpp"
#include "metis/infrastructure/toml_config_repository.hpp"

using namespace metis;

// Captures the last executed command so tests can assert on it.
struct MockShell : domain::ports::IShellExecutor {
  std::string last_cmd;

  std::string exec(const std::string&) override { return {}; }
  domain::ports::CapturedResult exec_captured(const std::string& cmd) override {
    last_cmd = cmd;
    return {0, {}};
  }
  bool command_exists(const std::string&) override { return true; }
};

class FileSystemTest : public ::testing::Test {
 protected:
  void SetUp() override {
    test_dir = std::filesystem::temp_directory_path() / "metis_fs_test";
    std::filesystem::create_directories(test_dir);
  }

  void TearDown() override { std::filesystem::remove_all(test_dir); }

  std::filesystem::path test_dir;
};

TEST_F(FileSystemTest, WriteFileCreateFile) {
  infrastructure::OsFileSystem fs;
  auto file_path = test_dir / "test.txt";
  std::string content = "woilah cik";

  bool result = fs.write_file(file_path, content);

  EXPECT_TRUE(result);
  EXPECT_TRUE(std::filesystem::exists(file_path));

  std::ifstream file(file_path);
  std::string read_content((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
  EXPECT_EQ(read_content, content);
}

TEST_F(FileSystemTest, ReadFileReturnsContent) {
  infrastructure::OsFileSystem fs;
  auto file_path = test_dir / "read_test.txt";
  std::string content = "hello world";

  ASSERT_TRUE(fs.write_file(file_path, content));
  auto result = fs.read_file(file_path);

  EXPECT_EQ(result, content);
}

TEST_F(FileSystemTest, ExistsDetectsFile) {
  infrastructure::OsFileSystem fs;
  auto file_path = test_dir / "exist_test.txt";

  EXPECT_FALSE(fs.exists(file_path));

  ASSERT_TRUE(fs.write_file(file_path, "content"));
  EXPECT_TRUE(fs.exists(file_path));
}

// Generic check abstraction: full field set parses from TOML.
TEST(ConfigCheckTest, ParsesAllCheckFields) {
  auto cfg_dir = std::filesystem::temp_directory_path() / "metis_check_test";
  std::filesystem::create_directories(cfg_dir);
  auto cfg_path = cfg_dir / ".metis.toml";
  infrastructure::OsFileSystem fs;
  ASSERT_TRUE(fs.write_file(cfg_path,
                            "[project]\n"
                            "name = \"demo\"\n\n"
                            "[[checks]]\n"
                            "name = \"clang-format\"\n"
                            "description = \"Format C++ files\"\n"
                            "enabled = false\n"
                            "command = \"clang-format\"\n"
                            "args = [\"-i\", \"-style=file\"]\n"
                            "patterns = [\"*.cpp\", \"*.hpp\"]\n"
                            "timeout = 30\n"
                            "severity = \"warning\"\n"));

  infrastructure::TomlConfigRepository repo(
      std::make_unique<infrastructure::OsFileSystem>(),
      std::make_unique<infrastructure::ProcessShellExecutor>());
  auto cfg = repo.load(cfg_path);

  ASSERT_EQ(cfg.checks.size(), 1u);
  const auto& check = cfg.checks.at(0);
  EXPECT_EQ(check.name, "clang-format");
  EXPECT_EQ(check.description, "Format C++ files");
  EXPECT_FALSE(check.enabled);
  EXPECT_EQ(check.command, "clang-format");
  ASSERT_EQ(check.args.size(), 2u);
  EXPECT_EQ(check.args.at(0), "-i");
  ASSERT_EQ(check.patterns.size(), 2u);
  EXPECT_EQ(check.patterns.at(1), "*.hpp");
  EXPECT_EQ(check.timeout, 30);
  EXPECT_EQ(check.severity, "warning");
}

// Generic check abstraction: ShellCheck (custom command) runs
// command + args + files via the shell and surfaces failing output.
TEST(ConfigCheckTest, ExecuteRunsCommand) {
  domain::config::Check config;
  config.command = "sh";
  config.args = {"-c", "echo hello; exit 3"};

  application::checks::ShellCheck check(config);
  infrastructure::ProcessShellExecutor shell;
  auto result = check.execute({"a.cpp"}, &shell, false, false);

  EXPECT_EQ(result.exit_code, 3);
  EXPECT_NE(result.output.find("hello"), std::string::npos);
}

// Command basename -> concrete check class dispatch used by run and sync.
TEST(MakeCheckTest, DispatchesByCommandBasename) {
  domain::config::Check base;
  auto classify = [&base](const std::string& command) {
    base.command = command;
    return std::string(typeid(*application::make_check(base)).name());
  };

  EXPECT_NE(classify("clang-format").find("ClangFormatCheck"), std::string::npos);
  EXPECT_NE(classify("clang-tidy").find("ClangTidyCheck"), std::string::npos);
  EXPECT_NE(classify("g++").find("CompilerCheck"), std::string::npos);
  EXPECT_NE(classify("gcc-14").find("CompilerCheck"), std::string::npos);
  EXPECT_NE(classify("cmake").find("BuildCheck"), std::string::npos);
  EXPECT_NE(classify("metis-security").find("SecurityCheck"), std::string::npos);
  // Unknown commands fall back to a generic shell check.
  EXPECT_NE(classify("my-linter").find("ShellCheck"), std::string::npos);
}

// Regression: config args may carry a bare "--" separating clang-tidy
// options from compile flags; source files must land BETWEEN them, otherwise
// clang-tidy sees zero inputs ("no input files specified").
TEST(ClangTidyCheckTest, PlacesFilesBeforeDoubleDash) {
  domain::config::Check config;
  config.command = "clang-tidy";
  config.args = {"--config-file=.clang-tidy", "--quiet", "--", "-std=c++20", "-Iinclude"};

  application::checks::ClangTidyCheck check(config);
  MockShell shell;
  (void)check.execute({"src/a.cpp", "src/b.cpp"}, &shell, false, false);

  EXPECT_EQ(shell.last_cmd,
            "clang-tidy '--config-file=.clang-tidy' '--quiet' 'src/a.cpp' 'src/b.cpp' -- "
            "'-std=c++20' '-Iinclude'");
}

// Without a separator, files are simply appended after the options.
TEST(ClangTidyCheckTest, AppendsFilesWithoutSeparator) {
  domain::config::Check config;
  config.command = "clang-tidy";
  config.args = {"--quiet"};

  application::checks::ClangTidyCheck check(config);
  MockShell shell;
  (void)check.execute({"a.cpp"}, &shell, false, false);

  EXPECT_EQ(shell.last_cmd, "clang-tidy '--quiet' 'a.cpp'");
}

// The TOML loader splits per-section parsers; this pins their combined
// behavior for [exclude], [output], [perf], and [execution].
TEST(ConfigSectionsTest, ParsesOptionalSections) {
  auto cfg_dir = std::filesystem::temp_directory_path() / "metis_sections_test";
  std::filesystem::create_directories(cfg_dir);
  auto cfg_path = cfg_dir / ".metis.toml";
  infrastructure::OsFileSystem fs;
  ASSERT_TRUE(fs.write_file(cfg_path,
                            "[project]\n"
                            "name = \"sections\"\n\n"
                            "[[checks]]\n"
                            "name = \"fmt\"\n"
                            "command = \"clang-format\"\n"
                            "patterns = [\"*.cpp\"]\n\n"
                            "[exclude]\n"
                            "paths = [\"build/\", \"tests/\"]\n\n"
                            "[output]\n"
                            "local_hook = false\n"
                            "github_actions = true\n"
                            "gitlab_ci = true\n\n"
                            "[perf]\n"
                            "enabled = true\n"
                            "build_dir = \"out\"\n"
                            "max_binary_size_mb = 12\n"
                            "max_build_time_sec = 90\n"
                            "benchmark_regex = \"bench\"\n\n"
                            "[execution]\n"
                            "parallel = false\n"));

  infrastructure::TomlConfigRepository repo(
      std::make_unique<infrastructure::OsFileSystem>(),
      std::make_unique<infrastructure::ProcessShellExecutor>());
  auto cfg = repo.load(cfg_path);

  ASSERT_EQ(cfg.exclude_paths.size(), 2u);
  EXPECT_EQ(cfg.exclude_paths.at(1), "tests/");
  EXPECT_FALSE(cfg.generate_local_hook);
  EXPECT_TRUE(cfg.generate_gha);
  EXPECT_TRUE(cfg.generate_gitlab_ci);
  EXPECT_TRUE(cfg.perf.enabled);
  EXPECT_EQ(cfg.perf.build_dir, "out");
  EXPECT_EQ(cfg.perf.max_binary_size_mb, 12u);
  EXPECT_EQ(cfg.perf.max_build_time_sec, 90u);
  EXPECT_EQ(cfg.perf.benchmark_regex, "bench");
  EXPECT_FALSE(cfg.parallel);
}

// Absent optional sections fall back to defaults: local hook only, parallel
// execution, perf disabled.
TEST(ConfigSectionsTest, DefaultsWhenSectionsMissing) {
  auto cfg_dir = std::filesystem::temp_directory_path() / "metis_defaults_test";
  std::filesystem::create_directories(cfg_dir);
  auto cfg_path = cfg_dir / ".metis.toml";
  infrastructure::OsFileSystem fs;
  ASSERT_TRUE(fs.write_file(cfg_path,
                            "[project]\n"
                            "name = \"minimal\"\n\n"
                            "[[checks]]\n"
                            "name = \"fmt\"\n"
                            "command = \"clang-format\"\n"
                            "patterns = [\"*.cpp\"]\n"));

  infrastructure::TomlConfigRepository repo(
      std::make_unique<infrastructure::OsFileSystem>(),
      std::make_unique<infrastructure::ProcessShellExecutor>());
  auto cfg = repo.load(cfg_path);

  EXPECT_TRUE(cfg.exclude_paths.empty());
  EXPECT_TRUE(cfg.generate_local_hook);
  EXPECT_FALSE(cfg.generate_gha);
  EXPECT_FALSE(cfg.generate_gitlab_ci);
  EXPECT_FALSE(cfg.perf.enabled);
  EXPECT_TRUE(cfg.parallel);
}

// CompilerCheck injects -fsyntax-only so a misconfigured check cannot emit
// object files; the compiled command must contain the flag.
TEST(ConfigCheckTest, CompilerCheckForcesSyntaxOnly) {
  domain::config::Check config;
  config.command = "g++";
  config.args = {"-std=c++20", "-Iinclude"};

  application::checks::CompilerCheck check(config);
  std::vector<std::string> args = check.arguments();
  EXPECT_NE(std::ranges::find(args, "-fsyntax-only"), args.end());
}
