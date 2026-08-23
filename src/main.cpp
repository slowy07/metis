#include <fmt/format.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "metis/application/dependency_check_use_case.hpp"
#include "metis/application/generate_workflow_use_case.hpp"
#include "metis/application/init_use_case.hpp"
#include "metis/application/install_toolchain_use_case.hpp"
#include "metis/application/install_use_case.hpp"
#include "metis/application/perf_checks_use_case.hpp"
#include "metis/application/run_checks_use_case.hpp"
#include "metis/application/sanitizer_checks_use_case.hpp"
#include "metis/application/test_checks_use_case.hpp"
#include "metis/argparse.hpp"
#include "metis/domain/config.hpp"
#include "metis/domain/error_codes.hpp"
#include "metis/domain/ports/toolchain_provider.hpp"
#include "metis/domain/workflow.hpp"
#include "metis/infrastructure/cli_git_repository.hpp"
#include "metis/infrastructure/os_file_system.hpp"
#include "metis/infrastructure/process_shell_executor.hpp"
#include "metis/infrastructure/toml_config_repository.hpp"
#include "metis/infrastructure/toolchain_factory.hpp"
#ifdef _WIN32
#include "metis/infrastructure/zip_archive_extractor.hpp"
#endif  // _WIN32
#include "metis/domain/ports/archive_extractor.hpp"
#include "metis/infrastructure/curl_http_client.hpp"
#include "metis/infrastructure/tar_archive_extractor.hpp"
#include "metis/presentation/interactive_init.hpp"

namespace {

using metis::application::InitOptions;

// Pre-parses --config/-c from argv before ArgParser runs.
// This is needed because the config path must be known before loading
// the config file, but ArgParser processes it as a regular option.
// lazy: duplicated parsing — ArgParser could support pre-parse hooks,
// but that's more code for a one-off need.
std::string preparse_config_path(std::span<char*> args) {
  std::string config_path = ".metis.toml";
  for (size_t i = 1; i + 1 < args.size(); ++i) {
    std::string_view arg = args[i];
    if ((arg == "-c" || arg == "--config") && i + 1 < args.size()) {
      config_path = args[i + 1];
      break;
    }
  }
  return config_path;
}

std::string to_lower_copy(std::string_view s) {
  std::string out(s);
  for (char& c : out) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  return out;
}

bool safe_stoi(const char* str, int& out) {
  try {
    size_t pos = 0;
    int val = std::stoi(str, &pos);
    if (pos != std::strlen(str)) {
      return false;
    }
    out = val;
    return true;
  } catch (...) {
    return false;
  }
}

struct InitStringFlag {
  std::string_view name;
  std::string InitOptions::* field;
  bool lower;
};

inline constexpr std::array<InitStringFlag, 10> k_init_string_flags = {
    InitStringFlag{.name = "--style", .field = &InitOptions::style, .lower = true},
    InitStringFlag{.name = "--name", .field = &InitOptions::project_name, .lower = false},
    InitStringFlag{
        .name = "--pointer-alignment", .field = &InitOptions::pointer_alignment, .lower = false},
    InitStringFlag{.name = "--brace-style", .field = &InitOptions::brace_style, .lower = false},
    InitStringFlag{.name = "--tidy-preset", .field = &InitOptions::tidy_preset, .lower = true},
    InitStringFlag{.name = "--tidy-severity", .field = &InitOptions::tidy_severity, .lower = true},
    InitStringFlag{
        .name = "--cmake-cpp-standard", .field = &InitOptions::cmake_cpp_standard, .lower = true},
    InitStringFlag{
        .name = "--cmake-target-type", .field = &InitOptions::cmake_target_type, .lower = true},
    InitStringFlag{.name = "--compiler", .field = &InitOptions::compiler, .lower = false},
    InitStringFlag{.name = "--compiler-cpp-standard",
                   .field = &InitOptions::compiler_cpp_standard,
                   .lower = true},
};

struct InitIntFlag {
  std::string_view name;
  int InitOptions::* field;
};

inline constexpr std::array<InitIntFlag, 3> k_init_int_flags = {
    InitIntFlag{.name = "--indent-width", .field = &InitOptions::indent_width},
    InitIntFlag{.name = "--column-limit", .field = &InitOptions::column_limit},
    InitIntFlag{.name = "--tidy-header-filter", .field = &InitOptions::tidy_header_filter},
};

// Applies a value-bearing init flag via the lookup tables; consumes its value
// by advancing i. Returns false when arg is not a table flag.
bool apply_value_flag(const std::string& arg, size_t& i, std::span<char*> args, InitOptions& opts) {
  for (const auto& flag : k_init_string_flags) {
    if (arg == flag.name && i + 1 < args.size()) {
      opts.*(flag.field) = flag.lower ? to_lower_copy(args[++i]) : std::string(args[++i]);
      return true;
    }
  }
  for (const auto& flag : k_init_int_flags) {
    if (arg == flag.name && i + 1 < args.size()) {
      safe_stoi(args[++i], opts.*(flag.field));
      return true;
    }
  }
  return false;
}

// Manually parses init-specific flags from argv.
// lazy: ArgParser handles subcommands but not value-bearing options for init.
// This is a second parser that runs after ArgParser identifies the subcommand.
bool parse_init_flags(std::span<char*> args, InitOptions& opts) {
  for (size_t i = 1; i < args.size(); ++i) {
    const std::string arg = args[i];
    if (apply_value_flag(arg, i, args, opts)) {
      continue;
    }

    if (arg == "--enable-clang-tidy" || arg == "--tidy") {
      opts.enable_clang_tidy = true;
    } else if (arg == "--enable-cmake" || arg == "--cmake") {
      opts.enable_cmake = true;
      opts.generate_source = true;
    } else if (arg == "--enable-conan") {
      opts.enable_conan = true;
    } else if (arg == "--cmake-enable-testing") {
      opts.cmake_enable_testing = true;
    } else if (arg == "--cmake-enable-sanitizers") {
      opts.cmake_enable_sanitizers = true;
    } else if (arg == "--add-dep" && i + 1 < args.size()) {
      opts.dependencies.emplace_back(args[++i]);
    } else if (arg == "--generate-src") {
      opts.generate_source = true;
    } else if (arg == "--enable-compiler-checks" || arg == "--compiler-checks") {
      opts.enable_compiler_checks = true;
    } else if (arg == "--compiler-werror") {
      opts.compiler_werror = true;
    } else if (arg == "--compiler-no-werror") {
      opts.compiler_werror = false;
    } else if (arg == "--compiler-debug-and-release") {
      opts.compiler_debug_and_release = true;
    } else if (arg == "--enable-security-checks" || arg == "--security-checks") {
      opts.enable_security_checks = true;
    }
  }
  return true;
}

// True when any argument in args (after argv[0]) is --verbose/-V.
bool scan_verbose_flag(std::span<char*> args) {
  bool verbose = false;
  for (size_t i = 1; i < args.size(); ++i) {
    std::string_view arg = args[i];
    if (arg == "--verbose" || arg == "-V") {
      verbose = true;
    }
  }
  return verbose;
}

int cmd_init(std::span<char*> args) {
  // Init needs its own filesystem and config repo instances because
  // it creates files (config, .clang-format, etc.) from templates.
  auto fs = std::make_unique<metis::infrastructure::OsFileSystem>();
  auto config_repo = std::make_unique<metis::infrastructure::TomlConfigRepository>(
      std::make_unique<metis::infrastructure::OsFileSystem>(),
      std::make_unique<metis::infrastructure::ProcessShellExecutor>());

  InitOptions opts;
  opts.project_name = fs->current_path().filename().string();

  // Interactive when explicitly requested or when no flags are present
  // (sensible default for new users); any flag → CLI-only mode.
  bool interactive = false;
  bool has_flags = false;
  for (size_t i = 1; i < args.size(); ++i) {
    std::string_view arg = args[i];
    if (arg == "--interactive" || arg == "-i") {
      interactive = true;
    }
    if (arg.starts_with('-')) {
      has_flags = true;
    }
  }

  if (interactive || !has_flags) {
    metis::presentation::run_interactive_init(opts);
  } else {
    parse_init_flags(args, opts);
  }

  auto cwd = fs->current_path();
  metis::application::InitUseCase init_use_case(std::move(config_repo), std::move(fs));
  auto result = init_use_case.execute(cwd, opts);

  if (!result.success) {
    std::cerr << "[ERROR] " << result.error_message << "\n";
    return static_cast<int>(metis::domain::ExitCode::GENERAL_ERROR);
  }

  metis::presentation::print_init_summary(opts, result);
  return static_cast<int>(metis::domain::ExitCode::SUCCESS);
}

int cmd_install(const metis::domain::config::ProjectConfig& cfg,
                const std::filesystem::path& repo_root) {
  metis::application::InstallUseCase install_use_case(
      std::make_unique<metis::infrastructure::OsFileSystem>(),
      std::make_unique<metis::infrastructure::CliGitRepository>(
          std::make_unique<metis::infrastructure::ProcessShellExecutor>()));
  auto result = install_use_case.execute(repo_root, cfg);

  if (!result.error_message.empty()) {
    std::cerr << "[ERROR] " << result.error_message << "\n";
    return static_cast<int>(metis::domain::ExitCode::HOOK_INSTALL_ERROR);
  }

  if (result.hook_installed) {
    std::cout << "[INFO] pre-commit hook installed at " << result.hook_path << "\n";
  }
  if (result.workflow_installed) {
    std::cout << "[INFO] workflow installed at " << result.workflow_path << "\n";
  }
  return static_cast<int>(metis::domain::ExitCode::SUCCESS);
}

int cmd_generate_workflow(const metis::domain::config::ProjectConfig& cfg,
                          const std::filesystem::path& repo_root,
                          metis::domain::workflow::Platform platform) {
  namespace wf = metis::domain::workflow;
  metis::application::GenerateWorkflowUseCase gen_use_case(
      std::make_unique<metis::infrastructure::OsFileSystem>());
  if (!gen_use_case.execute(cfg, repo_root, platform)) {
    std::cerr << "[ERROR] Failed to write workflow file\n";
    return static_cast<int>(metis::domain::ExitCode::WORKFLOW_GENERATION_ERROR);
  }

  const auto workflow_path = platform == wf::Platform::GitLabCI
                                 ? repo_root / ".gitlab-ci.yml"
                                 : repo_root / ".github" / "workflows" / "metis.yml";
  std::cout << "[INFO] Workflow generated at " << workflow_path.string() << "\n";
  return static_cast<int>(metis::domain::ExitCode::SUCCESS);
}

int cmd_test(std::span<char*> args, const metis::domain::config::ProjectConfig& cfg,
             const std::filesystem::path& repo_root) {
  using namespace metis;
  bool coverage = false;
  bool verbose = false;
  std::string build_dir_override;

  for (size_t i = 1; i < args.size(); ++i) {
    std::string_view arg = args[i];
    if (arg == "test") {
      continue;
    }
    if (arg == "--coverage") {
      coverage = true;
    } else if (arg == "--verbose" || arg == "-V") {
      verbose = true;
    } else if (!arg.starts_with('-')) {
      build_dir_override = std::string(arg);
    }
  }

  auto test_cfg = cfg;
  if (!build_dir_override.empty()) {
    test_cfg.test.build_dir = build_dir_override;
  }

  auto test_use_case =
      application::TestChecksUseCase(std::make_unique<infrastructure::ProcessShellExecutor>(),
                                     std::make_unique<infrastructure::OsFileSystem>());
  auto result = test_use_case.execute(test_cfg, repo_root, coverage, verbose);

  if (!result.output.empty()) {
    std::cout << result.output;
  }
  if (!result.success) {
    return static_cast<int>(domain::ExitCode::TEST_FAILURE);
  }
  if (coverage && !result.coverage_ok) {
    return static_cast<int>(domain::ExitCode::COVERAGE_THRESHOLD_NOT_MET);
  }
  return static_cast<int>(domain::ExitCode::SUCCESS);
}

int cmd_sanitizer(std::span<char*> args, const metis::domain::config::ProjectConfig& cfg,
                  const std::filesystem::path& repo_root) {
  using namespace metis;
  const bool verbose = scan_verbose_flag(args);

  if (!cfg.sanitizer.enabled) {
    std::cerr << "[WARN] Sanitizers not enabled in config.\n";
    return static_cast<int>(domain::ExitCode::SUCCESS);
  }

  auto sanitizer_use_case =
      application::SanitizerChecksUseCase(std::make_unique<infrastructure::ProcessShellExecutor>(),
                                          std::make_unique<infrastructure::OsFileSystem>());
  const bool ok = sanitizer_use_case.execute(cfg, repo_root, verbose);

  return static_cast<int>(ok ? domain::ExitCode::SUCCESS
                             : domain::ExitCode::SANITIZER_TEST_FAILURE);
}

int cmd_perf(std::span<char*> args, const metis::domain::config::ProjectConfig& cfg,
             const std::filesystem::path& repo_root) {
  using namespace metis;
  // Runs even with no flags: bare `metis perf` executes the perf checks.
  const bool verbose = scan_verbose_flag(args);

  auto perf_use_case =
      application::PerfChecksUseCase(std::make_unique<infrastructure::ProcessShellExecutor>(),
                                     std::make_unique<infrastructure::OsFileSystem>());
  auto result = perf_use_case.execute(cfg, repo_root, verbose);

  if (!result.output.empty()) {
    std::cout << result.output;
  }
  return static_cast<int>(result.success ? domain::ExitCode::SUCCESS
                                         : domain::ExitCode::PERF_CHECK_FAILURE);
}

int cmd_run(std::span<char*> args, const metis::domain::config::ProjectConfig& cfg,
            const std::filesystem::path& repo_root) {
  using namespace metis;
  // Parses its own flags (--all-files, --verbose, --dry-run, --format)
  // and builds a RunOptions struct for the use case.
  bool all_files = false;
  bool verbose = false;
  bool dry_run = false;
  bool format_mode = false;
  std::vector<std::string> run_files;

  for (size_t i = 1; i < args.size(); ++i) {
    std::string_view arg = args[i];
    if (arg == "run") {
      continue;
    }
    if (arg == "--all-files") {
      all_files = true;
    } else if (arg == "--verbose" || arg == "-V" || arg == "--detail") {
      verbose = true;
    } else if (arg == "--dry-run" || arg == "-n") {
      dry_run = true;
    } else if (arg == "--format" || arg == "-f") {
      format_mode = true;
    } else if (!arg.starts_with('-')) {
      run_files.emplace_back(arg);
    }
  }

  application::RunOptions opts;
  opts.verbose = verbose;
  opts.dry_run = dry_run;
  opts.mode = format_mode ? application::RunMode::FORMAT : application::RunMode::CHECK;

  if (all_files) {
    opts.source = application::FileSource::ALL_REPO;
  } else if (!run_files.empty()) {
    opts.source = application::FileSource::EXPLICIT;
    opts.explicit_files = std::move(run_files);
  } else {
    opts.source = application::FileSource::STAGED;
  }

  application::RunChecksUseCase run_use_case(
      std::make_unique<infrastructure::ProcessShellExecutor>(),
      std::make_unique<infrastructure::CliGitRepository>(
          std::make_unique<infrastructure::ProcessShellExecutor>()),
      std::make_unique<infrastructure::OsFileSystem>());
  return run_use_case.execute(cfg, opts);
}

constexpr auto k_std_17 = metis::domain::ports::CppStandard::CPP_17;
constexpr auto k_std_20 = metis::domain::ports::CppStandard::CPP_20;
constexpr auto k_std_23 = metis::domain::ports::CppStandard::CPP_23;
constexpr auto k_std_26 = metis::domain::ports::CppStandard::CPP_26;
inline constexpr std::array k_standards{std::pair{"17", k_std_17}, std::pair{"20", k_std_20},
                                        std::pair{"23", k_std_23}, std::pair{"26", k_std_26}};

int cmd_install_compiler(std::span<char*> args) {
  using namespace metis;
  std::string compiler = "gcc";
  std::string version;
  std::string cpp_standard_str = "20";
  std::string prefix;
  bool force = false;
  bool dry_run = false;

  for (size_t i = 1; i < args.size(); ++i) {
    std::string_view arg = args[i];
    if (arg == "--compiler" && i + 1 < args.size()) {
      compiler = args[++i];
    } else if (arg == "--version" && i + 1 < args.size()) {
      version = args[++i];
    } else if (arg == "--cpp-standard" && i + 1 < args.size()) {
      cpp_standard_str = args[++i];
    } else if (arg == "--prefix" && i + 1 < args.size()) {
      prefix = args[++i];
    } else if (arg == "--force") {
      force = true;
    } else if (arg == "--dry-run" || arg == "-n") {
      dry_run = true;
    }
  }

  const auto* const standard_it = std::ranges::find(k_standards, cpp_standard_str,
                                                    [](const auto& entry) { return entry.first; });
  if (standard_it == k_standards.end()) {
    std::cerr << "[ERROR] Invalid --cpp-standard. Use 17, 20, 23, or 26\n";
    return static_cast<int>(domain::ExitCode::INVALID_ARGUMENTS);
  }
  const auto cpp_standard = standard_it->second;

  auto shell = std::make_unique<infrastructure::ProcessShellExecutor>();
  auto fs = std::make_unique<infrastructure::OsFileSystem>();

  auto provider =
      infrastructure::ToolchainFactory::create(compiler, version, shell.get(), fs.get());
  if (!provider) {
    std::cerr << "[ERROR] Installation of '" << compiler << "' is not supported on this platform\n";
    return static_cast<int>(domain::ExitCode::UNSUPPORTED_PLATFORM);
  }

  if (!provider->supports_cpp_standard(cpp_standard)) {
    const auto max_std = provider->max_supported_standard();
    std::cerr << "[ERROR] C++ " << cpp_standard_str << " is not supported by this compiler. "
              << "Max supported: C++ " << static_cast<int>(max_std) << ".\n";
    return static_cast<int>(domain::ExitCode::UNSUPPORTED_CPP_STANDARD);
  }

  std::unique_ptr<domain::ports::IArchiveExtractor> extractor;
#ifdef _WIN32
  extractor = std::make_unique<infrastructure::ZipArchiveExtractor>(shell.get());
#else
  extractor = std::make_unique<infrastructure::TarArchiveExtractor>(shell.get());
#endif  // _WIN32
  auto http_client = std::make_unique<infrastructure::CurlHttpClient>(shell.get());

  application::InstallToolchainOptions opts;
  opts.compiler_ = compiler;
  opts.version_ = version;
  opts.cpp_standard_ = cpp_standard;
  opts.install_prefix_ = prefix;
  opts.force_ = force;
  opts.dry_run_ = dry_run;

  application::InstallToolchainUseCase use_case(std::move(provider), std::move(http_client),
                                                std::move(extractor), std::move(fs));
  auto result = use_case.execute(opts);

  if (result.was_already_installed_) {
    std::cout << "[INFO] " << result.error_message_ << "\n";
    return static_cast<int>(domain::ExitCode::SUCCESS);
  }
  if (!result.success_) {
    std::cerr << "[ERROR] " << result.error_message_ << "\n";
    return static_cast<int>(domain::ExitCode::TOOLCHAIN_INSTALL_ERROR);
  }

  std::cout << "[INFO] " << compiler << " " << result.version_ << " (C++ "
            << static_cast<int>(result.installed_cpp_standard_) << ") installed at "
            << result.installed_path_ << "\n";
  return static_cast<int>(domain::ExitCode::SUCCESS);
}

void print_deps_validations(const metis::domain::DependencyCheckResult& result, bool verbose) {
  std::cout << "\nDependencies\n";
  std::cout << "────────────\n";

  if (result.validations.empty()) {
    std::cout << "No dependency manifest found (conanfile.py, vcpkg.json, CMakeLists.txt)\n";
    return;
  }

  size_t name_width = 0;
  for (const auto& res_validation : result.validations) {
    name_width = std::max(name_width, res_validation.dep.name.size());
  }

  for (const auto& res_validation : result.validations) {
    std::cout << (res_validation.ok ? "✓ " : "✕ ") << res_validation.dep.name
              << std::string(name_width - res_validation.dep.name.size(), ' ');

    if (!res_validation.dep.version.empty()) {
      std::cout << "  " << res_validation.dep.version;
    }
    if (!res_validation.ok) {
      std::cout << "  [" << res_validation.message << "]";
    } else if (verbose) {
      std::cout << "  (" << res_validation.dep.source << ")";
    }
    std::cout << "\n";
  }
}

int cmd_deps(std::span<char*> args, const std::filesystem::path& repo_root) {
  using namespace metis;
  application::DependencyCheckOptions dep_opts;
  for (size_t i = 2; i < args.size(); ++i) {
    std::string_view arg = args[i];
    if (arg == "--verbose" || arg == "-V") {
      dep_opts.verbose = true;
    } else if (arg == "--graph" || arg == "-g") {
      dep_opts.generate_graph = true;
    }
  }

  application::DependencyCheckUseCase deps_uc(
      std::make_unique<infrastructure::ProcessShellExecutor>(),
      std::make_unique<infrastructure::OsFileSystem>());
  auto result = deps_uc.execute(repo_root, dep_opts);

  print_deps_validations(result, dep_opts.verbose);

  bool has_issue = false;
  if (!result.duplicates.empty()) {
    has_issue = true;
    std::cout << "\nDuplicate dependencies detected:\n";
    for (const auto& dep_duplicate : result.duplicates) {
      std::cout << "  - " << dep_duplicate << "\n";
    }
  }
  if (!result.lockfile_issues.empty()) {
    has_issue = true;
    std::cout << "\nLockfile issues:\n";
    for (const auto& lock_issue : result.lockfile_issues) {
      std::cout << "  - " << lock_issue << "\n";
    }
  }

  if (dep_opts.generate_graph && !result.validations.empty()) {
    std::cout << "\n[INFO] Dependency graph written to " << dep_opts.graph_output_path << "\n";
  }
  if (!has_issue && !result.validations.empty()) {
    std::cout << "\nNo dependency problems.\n";
  }

  return static_cast<int>(result.success() ? domain::ExitCode::SUCCESS
                                           : domain::ExitCode::MISSING_DEPENDENCY);
}

}  // namespace

int main(int argc, char** argv) {
  using namespace metis;

  auto argc_sz = static_cast<size_t>(argc);
  std::span<char*> args(argv, argc_sz);

  ArgParser app("metis", "Fast C++20-powered pre-commit & CI generator");
  app.set_version("0.4.0")
      .add_subcommand("init", "Create default .metis.toml")
      .add_subcommand("install", "Generate & install .git/hooks/pre-commit")
      .add_subcommand("generate-gha", "Output GitHub Actions workflow")
      .add_subcommand("generate-gitlab", "Output GitLab CI workflow")
      .add_subcommand("run", "Execute checks on files")
      .add_subcommand("install-compiler", "Download and install a C++ toolchain")
      .add_subcommand("sanitizer", "Run sanitizer checks (ASan, UBSan, TSan, LSan)")
      .add_subcommand("test", "Run test and optional coverage checks")
      .add_subcommand("deps", "Validate project dependencies (conan, vcpkg, cmake)")
      .add_subcommand("perf", "Run perfomance checks (build time, binary size, benchmarks)");

  std::string config_path = preparse_config_path(args);
  app.add_option("-c", "--config", "Config file path", config_path);

  if (!app.parse(argc, argv)) {
    return 0;
  }

  try {
    const auto subcmd = app.get_subcommand();

    if (subcmd == "init") {
      return cmd_init(args);
    }

    // Everything below needs the project config and repo root.
    auto config_repo = std::make_unique<infrastructure::TomlConfigRepository>(
        std::make_unique<infrastructure::OsFileSystem>(),
        std::make_unique<infrastructure::ProcessShellExecutor>());
    const auto cfg = config_repo->load(config_path);
    const auto repo_root = config_repo->find_git_root();

    if (subcmd == "install") {
      return cmd_install(cfg, repo_root);
    }
    if (subcmd == "generate-gha") {
      return cmd_generate_workflow(cfg, repo_root, domain::workflow::Platform::GithubAction);
    }
    if (subcmd == "generate-gitlab") {
      return cmd_generate_workflow(cfg, repo_root, domain::workflow::Platform::GitLabCI);
    }
    if (subcmd == "test") {
      return cmd_test(args, cfg, repo_root);
    }
    if (subcmd == "sanitizer") {
      return cmd_sanitizer(args, cfg, repo_root);
    }
    if (subcmd == "perf") {
      return cmd_perf(args, cfg, repo_root);
    }
    if (subcmd == "run") {
      return cmd_run(args, cfg, repo_root);
    }
    if (subcmd == "install-compiler") {
      return cmd_install_compiler(args);
    }
    if (subcmd == "deps") {
      return cmd_deps(args, repo_root);
    }

    app.show_help();
    return static_cast<int>(domain::ExitCode::INVALID_ARGUMENTS);

  } catch (const std::exception& error) {
    std::cerr << "[ERROR] " << error.what() << "\n";
    return static_cast<int>(domain::ExitCode::GENERAL_ERROR);
  } catch (...) {
    std::cerr << "[ERROR] Unknown fatal error occurred\n";
    return static_cast<int>(domain::ExitCode::GENERAL_ERROR);
  }
}
