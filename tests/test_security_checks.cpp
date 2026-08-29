#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "metis/application/checks/dependency_security_check.hpp"
#include "metis/application/checks/security_check.hpp"
#include "metis/domain/ports/shell_executor.hpp"

namespace {

namespace fs = std::filesystem;

struct FakeShell : metis::domain::ports::IShellExecutor {
  bool has_scanner_ = false;
  int scan_exit_ = 0;
  std::string scan_output_;

  std::string exec(const std::string& command) override { return command; }
  metis::domain::ports::CapturedResult exec_captured(const std::string& command) override {
    if (command.find("syft") != std::string::npos) {
      return {.exit_code_ = 0, .output_ = "[INFO] SBOM written to sbom.json\n"};
    }

    return {.exit_code_ = scan_exit_, .output_ = scan_output_.empty() ? command : scan_output_};
  }
  bool command_exists(const std::string& command) override {
    return (command == "osv-scanner" && has_scanner_) || command == "syft";
  }
};

class TempDir {
 public:
  TempDir() {
    path_ = fs::temp_directory_path() / "metis_security_tests";
    fs::remove_all(path_);
    fs::create_directories(path_);
  }

  ~TempDir() {
    std::error_code error_code;
    fs::remove_all(path_, error_code);
  }

  TempDir(const TempDir&) = delete;
  TempDir& operator=(const TempDir&) = delete;
  TempDir(TempDir&&) = delete;
  TempDir& operator=(TempDir&&) = delete;

  [[nodiscard]] std::string write(const std::string& name, const std::string& content) const {
    std::ofstream file(path_ / name);
    file << content;
    return (path_ / name).generic_string();
  }

  [[nodiscard]] const fs::path& path() const { return path_; }

 private:
  fs::path path_;
};

metis::domain::config::Check default_config() { return {}; }

TEST(SecurityCheckTest, DangerousFunctionIsDetectedWithLocation) {
  TempDir dir;
  auto file = dir.write("main.cpp", "int main() {\n  strcpy(dst, src);\n}\n");

  metis::application::checks::SecurityCheck check(default_config());
  auto result = check.execute({file}, nullptr, false, false);

  EXPECT_EQ(result.exit_code, 1);
  EXPECT_NE(result.output.find("dangerous-function"), std::string::npos);
  EXPECT_NE(result.output.find("main.cpp:2:"), std::string::npos);
}

TEST(SecurityCheckTest, CleanFilePasses) {
  TempDir dir;
  auto file = dir.write("clean.cpp", "int main() { return 0; }\n");

  metis::application::checks::SecurityCheck check(default_config());
  auto result = check.execute({file}, nullptr, false, false);

  EXPECT_EQ(result.exit_code, 0);
  EXPECT_TRUE(result.output.empty());
}

TEST(SecurityCheckTest, CommentLinesAreSkipped) {
  TempDir dir;
  auto file = dir.write("doc.cpp", "// strcpy(dst, src)\n/* sprintf(buf, x) */\n");

  metis::application::checks::SecurityCheck check(default_config());
  auto result = check.execute({file}, nullptr, false, false);

  EXPECT_EQ(result.exit_code, 0);
}

TEST(SecurityCheckTest, NonSourceFileIsSkipped) {
  TempDir dir;
  auto file = dir.write("notes.txt", "strcpy(dst, src)\napi_key = \"abc123\"\n");

  metis::application::checks::SecurityCheck check(default_config());
  auto result = check.execute({file}, nullptr, false, false);

  EXPECT_EQ(result.exit_code, 0);
}

TEST(SecurityCheckTest, HardcodedSecretIsDetectedCaseInsensitive) {
  TempDir dir;
  auto file = dir.write("creds.cpp", "API_KEY = \"abc123\";\n");

  metis::application::checks::SecurityCheck check(default_config());
  auto result = check.execute({file}, nullptr, false, false);

  EXPECT_EQ(result.exit_code, 1);
  EXPECT_NE(result.output.find("hardcoded-secret"), std::string::npos);
}

TEST(SecurityCheckTest, DryRunDoesNothing) {
  TempDir dir;
  auto file = dir.write("bad.cpp", "gets(buf);\n");

  metis::application::checks::SecurityCheck check(default_config());
  auto result = check.execute({file}, nullptr, false, true);

  EXPECT_EQ(result.exit_code, 0);
  EXPECT_TRUE(result.output.empty());
}

TEST(DependencySecurityCheckTest, NoScannerInstalledWarnsButPasses) {
  FakeShell shell;
  shell.has_scanner_ = false;

  metis::application::checks::DependencySecurityCheck check(default_config());
  auto result = check.execute({}, &shell, false, false);

  EXPECT_EQ(result.exit_code, 0);
  EXPECT_NE(result.output.find("[WARN] No CVE scanner found"), std::string::npos);
}

TEST(DependencySecurityCheckTest, CveInScanOutputFails) {
  FakeShell shell;
  shell.has_scanner_ = true;
  shell.scan_output_ = "Vulnerability found: CVE-2024-1234 in fmt 10.0.0\n";

  metis::application::checks::DependencySecurityCheck check(default_config());
  auto result = check.execute({}, &shell, false, false);

  EXPECT_EQ(result.exit_code, 1);
}

TEST(DependencySecurityCheckTest, CleanScanPasses) {
  FakeShell shell;
  shell.has_scanner_ = true;
  shell.scan_output_ = "0 vulnerabilities, all clear\n";

  metis::application::checks::DependencySecurityCheck check(default_config());
  auto result = check.execute({}, &shell, false, false);

  EXPECT_EQ(result.exit_code, 0);
}

TEST(DependencySecurityCheckTest, DryRunDoesNothing) {
  FakeShell shell;
  shell.has_scanner_ = true;
  shell.scan_output_ = "CVE-2024-1234";

  metis::application::checks::DependencySecurityCheck check(default_config());
  auto result = check.execute({}, &shell, false, true);

  EXPECT_EQ(result.exit_code, 0);
  EXPECT_TRUE(result.output.empty());
}

}  // namespace
