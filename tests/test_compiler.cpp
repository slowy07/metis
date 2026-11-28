#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <system_error>
#include <toml++/toml.hpp>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

#include "metis/application/emit_env_use_case.hpp"
#include "metis/application/list_compilers_use_case.hpp"
#include "metis/application/resolve_toolchain_use_case.hpp"
#include "metis/application/switch_toolchain_use_case.hpp"
#include "metis/domain/compiler/compiler.hpp"
#include "metis/domain/compiler/ports.hpp"
#include "metis/infrastructure/compiler/env_manager.hpp"
#include "metis/infrastructure/compiler/path_compiler_detector.hpp"
#include "metis/infrastructure/compiler/process_compiler_prober.hpp"
#include "metis/infrastructure/compiler/toml_compiler_registry.hpp"
#include "metis/infrastructure/compiler/toml_toolchain_config_repository.hpp"

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

struct FakeToolchainConfig final : dc::ToolchainConfigRepository {
  std::filesystem::path config_path = "/tmp/metis-test-toolchains.toml";
  std::optional<std::string> pinned;
  [[nodiscard]] std::filesystem::path path() const override { return config_path; }
  [[nodiscard]] std::optional<std::string> default_toolchain() const override { return pinned; }
  void set_default(std::string_view id) override { pinned = std::string(id); }
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
    auto now = std::chrono::steady_clock::now().time_since_epoch().count();
#ifdef _WIN32
    auto pid = static_cast<long long>(_getpid());
#else
    auto pid = static_cast<long long>(::getpid());
#endif
    path_ = base / ("metis-compiler-test-" + std::to_string(pid) + "-" +
                    std::to_string(counter_++) + "-" + std::to_string(now));
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
  FakeDetector detector;
  FakeRegistry registry;
  application::ListCompilersUseCase list(detector, registry);
  FakeToolchainConfig project;
  FakeToolchainConfig global;
  application::ResolveToolchainUseCase uc(list, project, global);
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
  FakeDetector detector;
  FakeRegistry registry;
  application::ListCompilersUseCase list(detector, registry);
  FakeToolchainConfig project;
  FakeToolchainConfig global;
  application::ResolveToolchainUseCase uc(list, project, global);
  auto found = uc.which("mycc");
  ASSERT_TRUE(found.has_value());
  EXPECT_EQ(std::filesystem::weakly_canonical(*found), std::filesystem::weakly_canonical(bin));
}

TEST(ResolveToolchainUseCaseTest, MissingReturnsNullopt) {
  ScopedTempDir tmp;
  ScopedEnvVar path("PATH", tmp.path().string());
  FakeDetector detector;
  FakeRegistry registry;
  application::ListCompilersUseCase list(detector, registry);
  FakeToolchainConfig project;
  FakeToolchainConfig global;
  application::ResolveToolchainUseCase uc(list, project, global);
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
  std::error_code err;
  for (const auto& p : {gcc, gxx}) {
    std::filesystem::permissions(p,
                                 std::filesystem::perms::owner_exec |
                                     std::filesystem::perms::owner_read |
                                     std::filesystem::perms::owner_write,
                                 std::filesystem::perm_options::add, err);
    ASSERT_FALSE(err);
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

TEST(SwitchToolchainUseCaseTest, UnknownIdThrows) {
  FakeDetector detector;
  FakeRegistry registry;
  application::ListCompilersUseCase list(detector, registry);
  application::SwitchToolchainUseCase use_case(list);
  FakeToolchainConfig repo;
  EXPECT_THROW(use_case.execute("missing", repo), std::runtime_error);
}

TEST(SwitchToolchainUseCaseTest, SetsDefaultOnKnownId) {
  FakeDetector detector;
  FakeRegistry registry;
  registry.compilers.push_back(make_compiler("gcc-13", "13.2.0", "system"));
  application::ListCompilersUseCase list(detector, registry);
  application::SwitchToolchainUseCase use_case(list);
  FakeToolchainConfig repo;
  use_case.execute("gcc-13", repo);
  ASSERT_TRUE(repo.default_toolchain().has_value());
  EXPECT_EQ(*repo.default_toolchain(), "gcc-13");
}

TEST(EmitEnvUseCaseTest, NoPinAndNoIdThrows) {
  FakeDetector detector;
  FakeRegistry registry;
  application::ListCompilersUseCase list(detector, registry);
  FakeToolchainConfig project;
  FakeToolchainConfig global;
  infrastructure::compiler::PosixEnvManager env_manager;
  application::EmitEnvUseCase use_case(list, project, global, env_manager);
  EXPECT_THROW((void)use_case.execute(std::nullopt), std::runtime_error);
}

TEST(EmitEnvUseCaseTest, UnknownIdThrows) {
  FakeDetector detector;
  FakeRegistry registry;
  application::ListCompilersUseCase list(detector, registry);
  FakeToolchainConfig project;
  FakeToolchainConfig global;
  infrastructure::compiler::PosixEnvManager env_manager;
  application::EmitEnvUseCase use_case(list, project, global, env_manager);
  EXPECT_THROW((void)use_case.execute(std::optional<std::string_view>("missing")),
               std::runtime_error);
}

TEST(EmitEnvUseCaseTest, ExplicitIdReturnsEnv) {
  FakeDetector detector;
  FakeRegistry registry;
  auto c = make_compiler("gcc-13", "13.2.0", "system");
  c.cc = "/opt/gcc/bin/gcc";
  c.cxx = "/opt/gcc/bin/g++";
  registry.compilers.push_back(c);
  application::ListCompilersUseCase list(detector, registry);
  FakeToolchainConfig project;
  FakeToolchainConfig global;
  infrastructure::compiler::PosixEnvManager env_manager;
  application::EmitEnvUseCase use_case(list, project, global, env_manager);
  auto vars = use_case.execute(std::optional<std::string_view>("gcc-13"));
  ASSERT_GE(vars.size(), 3u);
  EXPECT_EQ(vars[0].key, "CC");
  EXPECT_EQ(vars[0].value, "/opt/gcc/bin/gcc");
  EXPECT_EQ(vars[1].key, "CXX");
  EXPECT_EQ(vars[1].value, "/opt/gcc/bin/g++");
  EXPECT_EQ(vars.back().key, "METIS_TOOLCHAIN");
  EXPECT_EQ(vars.back().value, "gcc-13");
}

TEST(EmitEnvUseCaseTest, ProjectPinTakesPrecedenceOverGlobal) {
  FakeDetector detector;
  FakeRegistry registry;
  registry.compilers.push_back(make_compiler("gcc-13", "13", "system"));
  registry.compilers.push_back(make_compiler("clang-18", "18", "system"));
  application::ListCompilersUseCase list(detector, registry);
  FakeToolchainConfig project;
  project.pinned = "clang-18";
  FakeToolchainConfig global;
  global.pinned = "gcc-13";
  infrastructure::compiler::PosixEnvManager env_manager;
  application::EmitEnvUseCase use_case(list, project, global, env_manager);
  auto vars = use_case.execute(std::nullopt);
  ASSERT_FALSE(vars.empty());
  EXPECT_EQ(vars.back().value, "clang-18");
}

TEST(ResolveToolchainUseCaseTest, PinnedCompilerResolvesCcAndCxx) {
  FakeDetector detector;
  FakeRegistry registry;
  auto compiler = make_compiler("gcc-13", "13", "system");
  compiler.cc = "/opt/gcc/bin/gcc";
  compiler.cxx = "/opt/gcc/bin/g++";
  registry.compilers.push_back(compiler);
  application::ListCompilersUseCase list(detector, registry);
  FakeToolchainConfig project;
  project.pinned = "gcc-13";
  project.config_path = "/tmp/project-toolchains.toml";
  FakeToolchainConfig global;
  application::ResolveToolchainUseCase use_case(list, project, global);
  auto resolved_cc = use_case.which("cc");
  auto resolved_cxx = use_case.which("c++");
  ASSERT_TRUE(resolved_cc.has_value());
  ASSERT_TRUE(resolved_cxx.has_value());
  EXPECT_EQ(resolved_cc->string(), "/opt/gcc/bin/gcc");
  EXPECT_EQ(resolved_cxx->string(), "/opt/gcc/bin/g++");
}

TEST(ResolveToolchainUseCaseTest, ProjectPinTakesPrecedenceOverGlobal) {
  FakeDetector detector;
  FakeRegistry registry;
  auto project_compiler = make_compiler("gcc-13", "13", "system");
  project_compiler.cc = "/opt/project/gcc";
  project_compiler.cxx = "/opt/project/g++";
  auto global_compiler = make_compiler("clang-18", "18", "system");
  global_compiler.cc = "/opt/global/clang";
  global_compiler.cxx = "/opt/global/clang++";
  registry.compilers.push_back(project_compiler);
  registry.compilers.push_back(global_compiler);
  application::ListCompilersUseCase list(detector, registry);
  FakeToolchainConfig project;
  project.pinned = "gcc-13";
  FakeToolchainConfig global;
  global.pinned = "clang-18";
  application::ResolveToolchainUseCase use_case(list, project, global);
  auto resolved = use_case.which("gcc");
  ASSERT_TRUE(resolved.has_value());
  EXPECT_EQ(resolved->string(), "/opt/project/gcc");
}

TEST(ResolveToolchainUseCaseTest, MissingPinnedToolchainThrowsClearError) {
  FakeDetector detector;
  FakeRegistry registry;
  application::ListCompilersUseCase list(detector, registry);
  FakeToolchainConfig project;
  project.pinned = "missing-toolchain";
  project.config_path = "/tmp/project/.metis/toolchains.toml";
  FakeToolchainConfig global;
  application::ResolveToolchainUseCase use_case(list, project, global);
  try {
    (void)use_case.which("cc");
    FAIL() << "expected runtime_error";
  } catch (const std::runtime_error& e) {
    std::string msg = e.what();
    EXPECT_NE(msg.find("missing-toolchain"), std::string::npos);
    EXPECT_NE(msg.find("metis toolchain list"), std::string::npos);
  }
}

TEST(ResolveToolchainUseCaseTest, NonCcCommandFallsBackToPathSearch) {
  ScopedTempDir tmp;
  auto bin = tmp.path() / "mytool";
  {
    std::ofstream out(bin);
    out << "#!/bin/sh\\nexit 0\\n";
  }
  std::error_code err;
  std::filesystem::permissions(bin,
                               std::filesystem::perms::owner_exec |
                                   std::filesystem::perms::owner_read |
                                   std::filesystem::perms::owner_write,
                               std::filesystem::perm_options::add, err);
  ASSERT_FALSE(err);
  ScopedEnvVar path("PATH", tmp.path().string());

  FakeDetector detector;
  FakeRegistry registry;
  registry.compilers.push_back(make_compiler("gcc-13", "13", "system"));
  application::ListCompilersUseCase list(detector, registry);
  FakeToolchainConfig project;
  project.pinned = "gcc-13";
  FakeToolchainConfig global;
  application::ResolveToolchainUseCase use_case(list, project, global);
  auto found = use_case.which("mytool");
  ASSERT_TRUE(found.has_value());
}

TEST(ToolchainConfigRepositoryTest, SetDefaultCreatesAndReads) {
  ScopedTempDir tmp;
  auto path = tmp.path() / ".metis" / "toolchains.toml";
  infrastructure::compiler::TomlToolchainConfigRepository repo(path);
  EXPECT_FALSE(repo.default_toolchain().has_value());
  repo.set_default("gcc-13");
  auto def = repo.default_toolchain();
  ASSERT_TRUE(def.has_value());
  EXPECT_EQ(*def, "gcc-13");
}

TEST(ToolchainConfigRepositoryTest, PreservesExistingProjectKeys) {
  ScopedTempDir tmp;
  auto path = tmp.path() / "toolchains.toml";
  {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path);
    out << "[project]\nname = \"demo\"\ndefault = \"old\"\n";
  }
  infrastructure::compiler::TomlToolchainConfigRepository repo(path);
  repo.set_default("gcc-13");
  auto tbl = toml::parse_file(path.string());
  auto* project = tbl.get("project")->as_table();
  ASSERT_NE(project, nullptr);
  auto* name_node = project->get("name");
  auto* default_node = project->get("default");
  ASSERT_NE(name_node, nullptr);
  ASSERT_NE(default_node, nullptr);
  EXPECT_EQ(name_node->value_or(std::string{}), "demo");
  EXPECT_EQ(default_node->value_or(std::string{}), "gcc-13");
}

TEST(ToolchainConfigRepositoryTest, CorruptFileThrows) {
  ScopedTempDir tmp;
  auto path = tmp.path() / "toolchains.toml";
  {
    std::ofstream out(path);
    out << "not = [valid";
  }
  infrastructure::compiler::TomlToolchainConfigRepository repo(path);
  EXPECT_THROW((void)repo.default_toolchain(), std::runtime_error);
}

TEST(ToolchainConfigRepositoryTest, FindProjectConfigPathUsesGitRoot) {
  ScopedTempDir tmp;
  auto root = tmp.path() / "repo";
  auto nested = root / "src" / "deep";
  std::filesystem::create_directories(nested);
  std::filesystem::create_directories(root / ".git");
  auto found = infrastructure::compiler::find_project_config_path(nested);
  EXPECT_EQ(found, root / ".metis" / "toolchains.toml");
}

TEST(PosixEnvManagerTest, RendersShellEvalAbleExports) {
  dc::Compiler compiler;
  compiler.id = "gcc-13";
  compiler.cc = "/opt/gcc/bin/gcc";
  compiler.cxx = "/opt/gcc/bin/g++";
  infrastructure::compiler::PosixEnvManager mgr;
  auto out = infrastructure::compiler::render_posix_env(mgr.env_for(compiler));
  EXPECT_NE(out.find("export CC='/opt/gcc/bin/gcc'"), std::string::npos);
  EXPECT_NE(out.find("export CXX='/opt/gcc/bin/g++'"), std::string::npos);
  EXPECT_NE(out.find("export METIS_TOOLCHAIN='gcc-13'"), std::string::npos);
}

TEST(PosixEnvManagerTest, QuotesSingleQuotes) {
  EXPECT_EQ(infrastructure::compiler::shell_quote("a'b"), "'a'\\''b'");
}

#ifndef _WIN32
TEST(ToolchainCliIntegrationTest, UseEnvWhichAndEval) {
  const char* bin = METIS_BINARY_PATH;
  ASSERT_NE(bin, nullptr);

  ScopedTempDir tmp;
  auto project = tmp.path() / "project";
  auto nested = project / "src";
  std::filesystem::create_directories(nested);
  std::filesystem::create_directories(project / ".git");

  auto fakebin = tmp.path() / "bin";
  std::filesystem::create_directories(fakebin);
  auto fake_gcc = fakebin / "gcc";
  auto fake_gxx = fakebin / "g++";
  for (const auto& path_entry : {fake_gcc, fake_gxx}) {
    std::ofstream out(path_entry);
    out << "#!/bin/sh\n"
           "if [ \"$1\" = \"--version\" ]; then echo 'g++ (GCC) 13.2.0'; exit 0; fi\n"
           "if [ \"$1\" = \"-std=c++20\" ]; then exit 0; fi\n"
           "exit 0\n";
  }
  std::error_code err;
  for (const auto& path_entry : {fake_gcc, fake_gxx}) {
    std::filesystem::permissions(path_entry,
                                 std::filesystem::perms::owner_exec |
                                     std::filesystem::perms::owner_read |
                                     std::filesystem::perms::owner_write,
                                 std::filesystem::perm_options::add, err);
    ASSERT_FALSE(err);
  }

  ScopedEnvVar path("PATH", fakebin.string());
  ScopedEnvVar xdg_data("XDG_DATA_HOME", (tmp.path() / "xdg-data").string());
  ScopedEnvVar xdg_config("XDG_CONFIG_HOME", (tmp.path() / "xdg-config").string());
  ScopedEnvVar home("HOME", (tmp.path() / "home").string());

  auto run = [&](const std::string& args, const std::filesystem::path& cwd) {
    std::string cmd = "cd " + infrastructure::compiler::shell_quote(cwd.string()) + " && " +
                      infrastructure::compiler::shell_quote(bin) + " " + args + " 2>&1";
    std::array<char, 256> buf{};
    std::string out_str;
    FILE* pipe = popen(cmd.c_str(), "r");
    int status = -1;
    if (pipe != nullptr) {
      while (fgets(buf.data(), static_cast<int>(buf.size()), pipe) != nullptr) {
        out_str += buf.data();
      }
      status = pclose(pipe);
    }
    return std::pair{status, out_str};
  };

  auto [list_rc, list_out] = run("toolchain list", nested);
  ASSERT_EQ(list_rc, 0);
  ASSERT_NE(list_out.find("gcc-13.2.0"), std::string::npos);

  auto [use_rc, use_out] = run("toolchain use gcc-13.2.0", nested);
  EXPECT_EQ(use_rc, 0);
  EXPECT_TRUE(std::filesystem::exists(project / ".metis" / "toolchains.toml"));

  auto [use_global_rc, use_global_out] = run("toolchain use gcc-13.2.0 --global", nested);
  EXPECT_EQ(use_global_rc, 0);
  EXPECT_TRUE(std::filesystem::exists(tmp.path() / "xdg-config" / "metis" / "toolchains.toml"));

  auto [env_rc, env_out] = run("toolchain env gcc-13.2.0", nested);
  EXPECT_EQ(env_rc, 0);
  EXPECT_NE(env_out.find("export CC="), std::string::npos);
  EXPECT_NE(env_out.find("export CXX="), std::string::npos);

  auto [which_rc, which_out] = run("toolchain which cc", nested);
  EXPECT_EQ(which_rc, 0);
  EXPECT_NE(which_out.find("gcc"), std::string::npos);

  auto env_file = tmp.path() / "env.sh";
  {
    std::ofstream out(env_file);
    out << env_out;
  }
  std::string eval_cmd =
      "/bin/bash -c 'source \"$1\"; printf \"%s\\n%s\\n%s\" \"$CC\" \"$CXX\" \"$METIS_TOOLCHAIN\"' "
      "_ " +
      infrastructure::compiler::shell_quote(env_file.string()) + " 2>&1";
  std::string eval_out;
  FILE* eval_pipe = popen(eval_cmd.c_str(), "r");
  int eval_rc = -1;
  if (eval_pipe != nullptr) {
    std::array<char, 256> buf{};
    while (fgets(buf.data(), static_cast<int>(buf.size()), eval_pipe) != nullptr) {
      eval_out += buf.data();
    }
    eval_rc = pclose(eval_pipe);
  }
  EXPECT_EQ(eval_rc, 0);
  EXPECT_NE(eval_out.find(fake_gcc.string()), std::string::npos);
  EXPECT_NE(eval_out.find(fake_gxx.string()), std::string::npos);
  EXPECT_NE(eval_out.find("gcc-13.2.0"), std::string::npos);

  auto [bad_rc, bad_out] = run("toolchain use missing-toolchain", nested);
  EXPECT_NE(bad_rc, 0);
  EXPECT_NE(bad_out.find("unknown toolchain"), std::string::npos);
}
#endif
}  // namespace
