#include <gtest/gtest.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

#include "metis/application/list_compilers_use_case.hpp"
#include "metis/application/resolve_toolchain_use_case.hpp"
#include "metis/domain/compiler/compiler.hpp"
#include "metis/domain/compiler/ports.hpp"
#include "metis/infrastructure/compiler/path_compiler_detector.hpp"
#include "metis/infrastructure/compiler/process_compiler_prober.hpp"
#include "metis/infrastructure/compiler/toml_compiler_registry.hpp"

namespace {

using namespace metis;
namespace dc = domain::compiler;

struct FakeProber final : dc::CompilerProber {
  dc::ProbedCompiler probed;
  [[nodiscard]] dc::ProbedCompiler probe(const std::filesystem::path& /*cxx*/) const override {
    return probed;
  }
};

struct FakeDetector final : dc::CompilerDetector {
  std::vector<dc::Compiler> compilers;
  [[nodiscard]] std::vector<dc::Compiler> detect() const override { return compilers; }
};

struct FakeRegistry final : dc::CompilerRegistry {
  std::vector<dc::Compiler> compilers;
  [[nodiscard]] std::optional<dc::Compiler> find(std::string_view id) const override {
    for (const auto& c : compilers) {
      if (c.id == id) return c;
    }
    return std::nullopt;
  }
  [[nodiscard]] std::vector<dc::Compiler> list() const override { return compilers; }
  void save(const dc::Compiler& compiler) override {
    for (auto& c : compilers) {
      if (c.id == compiler.id) {
        c = compiler;
        return;
      }
    }
    compilers.push_back(compiler);
  }
  void remove(std::string_view id) override {
    compilers.erase(std::remove_if(compilers.begin(), compilers.end(),
                                   [&](const auto& c) { return c.id == id; }),
                    compilers.end());
  }
};

dc::Compiler make_compiler(std::string id, std::string version, std::string source) {
  dc::Compiler c;
  c.id = std::move(id);
  c.family = dc::CompilerFamily::kGcc;
  c.version.raw = std::move(version);
  c.cc = "/usr/bin/gcc";
  c.cxx = "/usr/bin/g++";
  c.source = std::move(source);
  c.capabilities.cxx20 = true;
  return c;
}

class ScopedTempDir {
 public:
  ScopedTempDir() {
    auto base = std::filesystem::temp_directory_path();
    path_ = base / ("metis-compiler-test-" + std::to_string(counter_++));
    std::filesystem::create_directories(path_);
    std::error_code ec;
    path_ = std::filesystem::weakly_canonical(path_, ec);
    if (ec) {
      ec.clear();
    }
  }
  ~ScopedTempDir() {
    std::error_code ec;
    std::filesystem::remove_all(path_, ec);
  }
  [[nodiscard]] const std::filesystem::path& path() const { return path_; }

 private:
  std::filesystem::path path_;
  static inline int counter_ = 0;
};

class ScopedEnvVar {
 public:
  ScopedEnvVar(const char* name, const std::string& value) : name_(name) {
    const char* old = std::getenv(name);
    if (old != nullptr) {
      had_old_ = true;
      old_ = old;
    }
#ifdef _WIN32
    _putenv_s(name_.c_str(), value.c_str());
#else
    setenv(name_.c_str(), value.c_str(), 1);
#endif
  }
  ~ScopedEnvVar() {
#ifdef _WIN32
    if (had_old_) {
      _putenv_s(name_.c_str(), old_.c_str());
    } else {
      _putenv_s(name_.c_str(), "");
    }
#else
    if (had_old_) {
      setenv(name_.c_str(), old_.c_str(), 1);
    } else {
      unsetenv(name_.c_str());
    }
#endif
  }

 private:
  std::string name_;
  bool had_old_ = false;
  std::string old_;
};

TEST(TomlCompilerRegistryTest, MissingFileReturnsEmpty) {
  ScopedTempDir tmp;
  infrastructure::compiler::TomlCompilerRegistry reg(tmp.path() / "missing.toml");
  EXPECT_TRUE(reg.list().empty());
  EXPECT_FALSE(reg.find("gcc").has_value());
}

TEST(TomlCompilerRegistryTest, SaveFindListRemoveRoundTrip) {
  ScopedTempDir tmp;
  infrastructure::compiler::TomlCompilerRegistry reg(tmp.path() / "compilers.toml");

  auto c = make_compiler("gcc-14", "14.2.0", "system");
  reg.save(c);

  auto found = reg.find("gcc-14");
  ASSERT_TRUE(found.has_value());
  EXPECT_EQ(found->id, "gcc-14");
  EXPECT_EQ(found->version.raw, "14.2.0");
  EXPECT_TRUE(found->capabilities.cxx20);

  auto list = reg.list();
  ASSERT_EQ(list.size(), 1u);
  EXPECT_EQ(list[0].id, "gcc-14");

  reg.remove("gcc-14");
  EXPECT_TRUE(reg.list().empty());
}

TEST(TomlCompilerRegistryTest, SaveUpdatesExisting) {
  ScopedTempDir tmp;
  infrastructure::compiler::TomlCompilerRegistry reg(tmp.path() / "compilers.toml");
  auto c = make_compiler("gcc-14", "14.1.0", "system");
  reg.save(c);
  c.version.raw = "14.2.0";
  reg.save(c);
  auto list = reg.list();
  ASSERT_EQ(list.size(), 1u);
  EXPECT_EQ(list[0].version.raw, "14.2.0");
}

TEST(TomlCompilerRegistryTest, CorruptFileThrows) {
  ScopedTempDir tmp;
  auto path = tmp.path() / "compilers.toml";
  {
    std::ofstream out(path);
    out << "not = [valid toml";
  }
  infrastructure::compiler::TomlCompilerRegistry reg(path);
  EXPECT_THROW((void)reg.list(), std::runtime_error);
}

TEST(ListCompilersUseCaseTest, InstalledOnlyReturnsRegistry) {
  FakeDetector detector;
  detector.compilers.push_back(make_compiler("gcc-14", "14", "system"));
  FakeRegistry registry;
  registry.compilers.push_back(make_compiler("clang-18", "18", "registry"));

  application::ListCompilersUseCase use_case(detector, registry);
  auto out = use_case.execute(true);
  ASSERT_EQ(out.size(), 1u);
  EXPECT_EQ(out[0].id, "clang-18");
}

TEST(ListCompilersUseCaseTest, MergesAndDedupesById) {
  FakeDetector detector;
  detector.compilers.push_back(make_compiler("gcc-14", "14", "system"));
  detector.compilers.push_back(make_compiler("clang-18", "18", "system"));
  FakeRegistry registry;
  registry.compilers.push_back(make_compiler("gcc-14", "14-reg", "registry"));

  application::ListCompilersUseCase use_case(detector, registry);
  auto out = use_case.execute(false);
  ASSERT_EQ(out.size(), 2u);
  EXPECT_EQ(out[0].id, "clang-18");
  EXPECT_EQ(out[1].id, "gcc-14");
  EXPECT_EQ(out[1].source, "registry");
}

TEST(ResolveToolchainUseCaseTest, EmptyReturnsNullopt) {
  application::ResolveToolchainUseCase uc;
  EXPECT_FALSE(uc.which("").has_value());
}

TEST(ResolveToolchainUseCaseTest, FindsExecutableInPath) {
  ScopedTempDir tmp;
  auto bin = tmp.path() / "mycc";
  {
    std::ofstream out(bin);
    out << "#!/bin/sh\nexit 0\n";
  }
  std::error_code ec;
  std::filesystem::permissions(bin,
                               std::filesystem::perms::owner_exec |
                                   std::filesystem::perms::owner_read |
                                   std::filesystem::perms::owner_write,
                               std::filesystem::perm_options::add, ec);
  ASSERT_FALSE(ec);

  ScopedEnvVar path("PATH", tmp.path().string());
  application::ResolveToolchainUseCase uc;
  auto found = uc.which("mycc");
  ASSERT_TRUE(found.has_value());
  EXPECT_EQ(std::filesystem::weakly_canonical(*found), std::filesystem::weakly_canonical(bin));
}

TEST(ResolveToolchainUseCaseTest, MissingReturnsNullopt) {
  ScopedTempDir tmp;
  ScopedEnvVar path("PATH", tmp.path().string());
  application::ResolveToolchainUseCase uc;
  EXPECT_FALSE(uc.which("definitely-not-a-real-compiler-xyz").has_value());
}

TEST(ProcessCompilerProberTest, MissingBinaryReturnsEmpty) {
  infrastructure::compiler::ProcessCompilerProber prober;
  auto probed = prober.probe("/definitely/not/a/compiler");
  EXPECT_TRUE(probed.version.raw.empty());
  EXPECT_FALSE(probed.capabilities.cxx20);
}

#ifndef _WIN32
TEST(PathCompilerDetectorTest, DetectsFakeCompilersFromPath) {
  ScopedTempDir tmp;
  auto gcc = tmp.path() / "gcc";
  auto gxx = tmp.path() / "g++";
  for (const auto& p : {gcc, gxx}) {
    std::ofstream out(p);
    out << "#!/bin/sh\n"
           "if [ \"$1\" = \"--version\" ]; then echo 'g++ (GCC) 14.2.0'; exit 0; fi\n"
           "if [ \"$1\" = \"-std=c++20\" ]; then exit 0; fi\n"
           "exit 0\n";
  }
  std::error_code ec;
  for (const auto& p : {gcc, gxx}) {
    std::filesystem::permissions(p,
                                 std::filesystem::perms::owner_exec |
                                     std::filesystem::perms::owner_read |
                                     std::filesystem::perms::owner_write,
                                 std::filesystem::perm_options::add, ec);
    ASSERT_FALSE(ec);
  }

  ScopedEnvVar path("PATH", tmp.path().string());
  infrastructure::compiler::ProcessCompilerProber prober;
  infrastructure::compiler::PathCompilerDetector detector(prober);
  auto compilers = detector.detect();
  ASSERT_FALSE(compilers.empty());
  EXPECT_EQ(compilers[0].family, dc::CompilerFamily::kGcc);
  EXPECT_NE(compilers[0].id.find("gcc-"), std::string::npos);
  EXPECT_TRUE(compilers[0].capabilities.cxx20);
}
#endif

}  // namespace
