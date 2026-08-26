#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <utility>

#include "metis/application/perf_checks_use_case.hpp"
#include "metis/domain/config.hpp"
#include "metis/domain/ports/file_system.hpp"
#include "metis/domain/ports/shell_executor.hpp"

namespace {

using namespace metis;

// the use case taking ownership of a copy.
struct MockShell : domain::ports::IShellExecutor {
  int exit_code = 0;
  std::string output;
  std::string* last_cmd_sink = nullptr;

  std::string exec(const std::string&) override { return {}; }
  domain::ports::CapturedResult exec_captured(const std::string& cmd) override {
    if (last_cmd_sink != nullptr) {
      *last_cmd_sink = cmd;
    }
    return {exit_code, output};
  }
  bool command_exists(const std::string&) override { return true; }
};

struct MockFs : domain::ports::IFileSystem {
  std::set<std::string> existing;

  bool exists(const std::filesystem::path& p) override { return existing.contains(p.string()); }
  bool create_directories(const std::filesystem::path&) override { return true; }
  bool write_file(const std::filesystem::path&, const std::string&) override { return true; }
  std::string read_file(const std::filesystem::path&) override { return {}; }
  bool remove(const std::filesystem::path&) override { return true; }
  bool set_permissions(const std::filesystem::path&, std::filesystem::perms,
                       std::filesystem::perm_options) override {
    return true;
  }
  std::filesystem::path current_path() override { return "/mock"; }
  std::filesystem::path absolute(const std::filesystem::path& path) override { return path; }
};

struct PerfChecksTest : ::testing::Test {
  MockShell shell;
  MockFs fs;
  domain::config::ProjectConfig cfg;
  std::filesystem::path root;
  std::string last_cmd;

  void SetUp() override {
    shell.last_cmd_sink = &last_cmd;
    cfg.perf.enabled = true;
    root = std::filesystem::temp_directory_path() / "metis_perf_tests";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "build");
    fs.existing.insert((root / "build").string());
  }

  void TearDown() override { std::filesystem::remove_all(root); }

  application::PerfResult run(bool verbose = false) {
    application::PerfChecksUseCase use_case(std::make_unique<MockShell>(shell),
                                            std::make_unique<MockFs>(fs));
    return use_case.execute(cfg, root, verbose);
  }

  static void write_bytes(const std::filesystem::path& p, std::size_t n) {
    std::ofstream f(p, std::ios::binary);
    f << std::string(n, 'x');
  }
};

TEST_F(PerfChecksTest, DisabledReturnsSuccessWithWarning) {
  cfg.perf.enabled = false;

  auto result = run();

  EXPECT_TRUE(result.success);
  EXPECT_NE(result.output.find("not enabled"), std::string::npos);
}

TEST_F(PerfChecksTest, MissingBuildDirFails) {
  fs.existing.clear();

  auto result = run();

  EXPECT_FALSE(result.success);
  EXPECT_NE(result.output.find("Build Directory"), std::string::npos);
}

TEST_F(PerfChecksTest, FailedBuildMarksBuildTimeNotOk) {
  cfg.perf.max_build_time_sec = 60;
  shell.exit_code = 1;

  auto result = run();

  EXPECT_FALSE(result.success);
  EXPECT_FALSE(result.build_time_ok);
  EXPECT_NE(result.output.find("Build failed during performance measurement"), std::string::npos);
}

TEST_F(PerfChecksTest, FastBuildPassesAndEchoesCommandWhenVerbose) {
  cfg.perf.max_build_time_sec = 600;

  // second threshold without sleeping >1s; only the pass branch is asserted.
  auto result = run(/*verbose=*/true);

  EXPECT_TRUE(result.success);
  EXPECT_TRUE(result.build_time_ok);
  EXPECT_NE(last_cmd.find("cmake --build"), std::string::npos);
  EXPECT_NE(result.output.find("$ cmake --build"), std::string::npos);
}

TEST_F(PerfChecksTest, BinaryWithinThresholdReportsSize) {
  constexpr std::size_t kSize = 12345;
  write_bytes(root / "app.bin", kSize);
  fs.existing.insert((root / "app.bin").string());
  cfg.perf.binary_path = "app.bin";
  cfg.perf.max_binary_size_mb = 1;

  auto result = run();

  EXPECT_TRUE(result.success);
  EXPECT_TRUE(result.binary_size_ok);
  EXPECT_EQ(result.binary_size_bytes, kSize);
}

TEST_F(PerfChecksTest, BinaryOverThresholdFails) {
  write_bytes(root / "big.bin", 1024 * 1024 + 1);
  fs.existing.insert((root / "big.bin").string());
  cfg.perf.binary_path = "big.bin";
  cfg.perf.max_binary_size_mb = 1;

  auto result = run();

  EXPECT_FALSE(result.success);
  EXPECT_FALSE(result.binary_size_ok);
  EXPECT_EQ(result.binary_size_bytes, 1024 * 1024 + 1);
}

TEST_F(PerfChecksTest, MissingBinaryFails) {
  cfg.perf.binary_path = "nope.bin";
  cfg.perf.max_binary_size_mb = 1;

  auto result = run();

  EXPECT_FALSE(result.success);
  EXPECT_FALSE(result.binary_size_ok);
  EXPECT_NE(result.output.find("binary not found"), std::string::npos);
}

TEST_F(PerfChecksTest, BenchmarksPassRunsCtestWithRegex) {
  cfg.perf.benchmark_regex = "bench_";

  auto result = run();

  EXPECT_TRUE(result.success);
  EXPECT_TRUE(result.benchmark_ok);
  EXPECT_NE(last_cmd.find("ctest --test-dir"), std::string::npos);
  EXPECT_NE(last_cmd.find("-R \"bench_\""), std::string::npos);
}

TEST_F(PerfChecksTest, BenchmarkFailureFails) {
  cfg.perf.benchmark_regex = "bench_";
  shell.exit_code = 1;

  auto result = run();

  EXPECT_FALSE(result.success);
  EXPECT_FALSE(result.benchmark_ok);
  EXPECT_NE(result.output.find("[ERROR] Benchmark execution failed"), std::string::npos);
}

TEST_F(PerfChecksTest, QuickLevelRunsSizeCheckOnly) {
  cfg.perf.max_build_time_sec = 10;
  cfg.perf.benchmark_regex = "bench_";
  fs.existing.clear();  // quick mode must not require a build dir

  application::PerfChecksUseCase use_case(std::make_unique<MockShell>(shell),
                                          std::make_unique<MockFs>(fs));
  auto result = use_case.execute(cfg, root, false, application::PerfLevel::QUICK);

  EXPECT_TRUE(result.success);
  EXPECT_TRUE(last_cmd.empty());  // no shell work: no rebuild, no ctest
}

TEST_F(PerfChecksTest, DefaultLevelIsFull) {
  cfg.perf.max_build_time_sec = 10;
  cfg.perf.benchmark_regex = "bench_";

  auto result = run();  // no level argument

  EXPECT_NE(last_cmd.find("ctest --test-dir"), std::string::npos);
  EXPECT_TRUE(result.success);
}

}  // namespace
