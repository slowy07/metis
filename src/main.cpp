#include <fmt/format.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <regex>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "metis/application/build_use_case.hpp"
#include "metis/application/dependency_check_use_case.hpp"
#include "metis/application/dependency_manage_use_case.hpp"
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
#include "metis/generators/clang_format_generator.hpp"
#include "metis/generators/clang_tidy_generator.hpp"
#include "metis/infrastructure/check_cache.hpp"
#include "metis/infrastructure/cli_git_repository.hpp"
#include "metis/infrastructure/cmake_dependency_provider.hpp"
#include "metis/infrastructure/conan_dependency_provider.hpp"
#include "metis/infrastructure/os_file_system.hpp"
#include "metis/infrastructure/process_shell_executor.hpp"
#include "metis/infrastructure/toml_config_repository.hpp"
#include "metis/infrastructure/toolchain_factory.hpp"
#include "metis/infrastructure/vcpkg_dependency_provider.hpp"
#include "metis/presentation/console.hpp"
#ifdef _WIN32
#include "metis/infrastructure/zip_archive_extractor.hpp"
#endif  // _WIN32
#include "metis/domain/ports/archive_extractor.hpp"
#include "metis/infrastructure/curl_http_client.hpp"
#include "metis/infrastructure/tar_archive_extractor.hpp"
#include "metis/presentation/interactive_init.hpp"
#include "metis/presentation/summary_reporter.hpp"

namespace {

using metis::presentation::Console;
using metis::presentation::SummaryReporter;

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

bool parse_init_flags(std::span<char*> args, metis::application::InitOptions& opts) {
  auto to_lower = [](std::string s) {
    for (char& chr : s) {
      chr = static_cast<char>(std::tolower(static_cast<unsigned char>(chr)));
    }
    return s;
  };

  auto safe_stoi = [](const char* str, int& out) -> bool {
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
  };

  for (size_t i = 1; i < args.size(); ++i) {
    std::string arg = args[i];
    if (arg == "--style" && i + 1 < args.size()) {
      opts.style = to_lower(args[++i]);
    } else if (arg == "--name" && i + 1 < args.size()) {
      opts.project_name = args[++i];
    } else if (arg == "--indent-width" && i + 1 < args.size()) {
      safe_stoi(args[++i], opts.indent_width);
    } else if (arg == "--column-limit" && i + 1 < args.size()) {
      safe_stoi(args[++i], opts.column_limit);
    } else if (arg == "--pointer-alignment" && i + 1 < args.size()) {
      opts.pointer_alignment = args[++i];
    } else if (arg == "--brace-style" && i + 1 < args.size()) {
      opts.brace_style = args[++i];
    } else if (arg == "--enable-clang-tidy" || arg == "--tidy") {
      opts.enable_clang_tidy = true;
    } else if (arg == "--tidy-preset" && i + 1 < args.size()) {
      opts.tidy_preset = to_lower(args[++i]);
    } else if (arg == "--tidy-severity" && i + 1 < args.size()) {
      opts.tidy_severity = to_lower(args[++i]);
    } else if (arg == "--tidy-header-filter" && i + 1 < args.size()) {
      safe_stoi(args[++i], opts.tidy_header_filter);
    } else if (arg == "--enable-cmake" || arg == "--cmake") {
      opts.enable_cmake = true;
      opts.generate_source = true;
    } else if (arg == "--enable-conan") {
      opts.enable_conan = true;
    } else if (arg == "--cmake-cpp-standard" && i + 1 < args.size()) {
      opts.cmake_cpp_standard = to_lower(args[++i]);
    } else if (arg == "--cmake-target-type" && i + 1 < args.size()) {
      opts.cmake_target_type = to_lower(args[++i]);
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
    } else if (arg == "--compiler" && i + 1 < args.size()) {
      opts.compiler = args[++i];
    } else if (arg == "--compiler-cpp-standard" && i + 1 < args.size()) {
      opts.compiler_cpp_standard = to_lower(args[++i]);
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

}  // namespace

int main(int argc, char** argv) {
  using namespace metis;

  auto argc_sz = static_cast<size_t>(argc);
  std::span<char*> args(argv, argc_sz);
  std::string config_path = preparse_config_path(args);

  ArgParser app("metis", "Fast C++20-powered pre-commit & CI generator");
  app.set_version("0.5.0")
      .add_option("-c", "--config", "Config file path", config_path)
      .add_subcommand("init", "Create default .metis.toml")
      .add_subcommand("install", "Generate & install .git/hooks/pre-commit")
      .add_subcommand("sync", "Sync environment: refresh hooks/workflows + validate checks")
      .add_subcommand("generate-gha", "Output GitHub Actions workflow")
      .add_subcommand("generate-gitlab", "Output GitLab CI workflow")
      .add_subcommand("run", "Execute checks on files")
      .add_subcommand("install-compiler", "Download and install a C++ toolchain")
      .add_subcommand("sanitizer", "Run sanitizer checks (ASan, UBSan, TSan, LSan)")
      .add_subcommand("test", "Run test and optional coverage checks")
      .add_subcommand("deps", "Validate project dependencies (conan, vcpkg, cmake)")
      .add_subcommand("build", "Configure and build the project with CMake")
      .add_subcommand("perf",
                      "Run performance checks (build time, binary size, benchmarks); "
                      "--level quick = binary size only [default: full]");

  app.set_subcommand_help(
      "run",
      "Usage:\n"
      "  metis run [OPTIONS] [FILES...]\n\n"
      "Options:\n"
      "  --all-files              Run checks on all tracked files (cache disabled)\n"
      "  --diff-only              Run checks on the diff only (staged files, cache enabled)\n"
      "  --verbose, -V, --detail  Show detailed output\n"
      "  --dry-run, -n            Show what would be run without executing\n"
      "  --format, -f             Run in formatting mode (apply fixes)\n\n"
      "  --no-cache               Bypass check cache, force re-check\n\n"
      "Configuration (.metis.toml):\n"
      "  [[checks]]\n"
      "    name        = \"...\"         # Check identifier\n"
      "    command     = \"...\"         # Executable to run\n"
      "    args        = [...]         # Arguments passed to command\n"
      "    patterns    = [\"*.cpp\"]     # File patterns to match\n"
      "    enabled     = true          # Whether check is active\n"
      "    timeout     = 0             # Timeout in seconds (0 = none)\n"
      "    severity    = \"error\"       # Failure severity level\n\n"
      "  [execution]\n"
      "    parallel    = true          # Run checks concurrently\n\n"
      "  [exclude]\n"
      "    paths       = [\"build/\", \"third_party/\"]\n");

  app.set_subcommand_help("test",
                          "Usage:\n"
                          "  metis test [OPTIONS] [BUILD_DIR]\n\n"
                          "Options:\n"
                          "  --coverage               Enable coverage reporting\n"
                          "  --verbose, -V            Show detailed output\n\n"
                          "Configuration (.metis.toml):\n"
                          "  [test]\n"
                          "    build_dir          = \"build\"      # Build directory for tests\n"
                          "    coverage           = false          # Enable coverage by default\n"
                          "    line_threshold     = 80.0           # Minimum line coverage %\n"
                          "    branch_threshold   = 70.0           # Minimum branch coverage %\n"
                          "    function_threshold = 90.0           # Minimum function coverage %\n"
                          "    timeout            = 0              # Test timeout in seconds\n");

  app.set_subcommand_help("build",
                          "Usage:\n"
                          "  metis build [OPTIONS]\n\n"
                          "Options:\n"
                          "  --build-dir <dir>        Build directory [default: build]\n"
                          "  --clean                  Clean before build\n"
                          "  --verbose, -V            Show detailed output\n"
                          "  -j, --jobs <n>           Parallel build jobs\n");

  app.set_subcommand_help("sanitizer",
                          "Usage:\n"
                          "  metis sanitizer [OPTIONS]\n\n"
                          "Options:\n"
                          "  --verbose, -V            Show detailed output\n\n"
                          "Configuration (.metis.toml):\n"
                          "  [sanitizer]\n"
                          "    enabled   = false                 # Enable sanitizer checks\n"
                          "    types     = [\"address\", \"undefined\"]  # Sanitizer types to run\n"
                          "    build_dir = \"build\"               # Build directory\n"
                          "    timeout   = 0                     # Timeout in seconds\n");

  app.set_subcommand_help(
      "perf",
      "Usage:\n"
      "  metis perf [OPTIONS]\n\n"
      "Options:\n"
      "  --verbose, -V            Show detailed output\n"
      "  --level <quick|full>     Check level [default: full]\n\n"
      "Configuration (.metis.toml):\n"
      "  [perf]\n"
      "    enabled            = false         # Enable performance checks\n"
      "    build_dir          = \"build\"       # Build directory\n"
      "    binary_path        = \"...\"         # Path to built binary\n"
      "    max_binary_size_mb = 0             # Max binary size in MB (0 = no limit)\n"
      "    max_build_time_sec = 0             # Max build time in seconds (0 = no limit)\n"
      "    benchmark_regex    = \"\"            # Regex to match benchmark names\n");

  app.set_subcommand_help(
      "init",
      "Usage:\n"
      "  metis init [OPTIONS]\n\n"
      "Options:\n"
      "  --style <google|llvm|...>              Clang-format style [default: google]\n"
      "  --name <project-name>                  Project name\n"
      "  --indent-width <n>                     Indentation width\n"
      "  --column-limit <n>                     Maximum column width\n"
      "  --pointer-alignment <Left|Right|Middle>  Pointer alignment\n"
      "  --brace-style <Attach|Allman|...>        Brace style\n"
      "  --enable-clang-tidy, --tidy            Enable clang-tidy checks\n"
      "  --tidy-preset <minimal|standard|strict>  Tidy preset\n"
      "  --tidy-severity <note|warning|error>     Tidy severity\n"
      "  --tidy-header-filter <0|1|2>           Header filter level\n"
      "  --enable-cmake, --cmake                Generate CMakeLists.txt\n"
      "  --enable-conan                         Generate conanfile.py\n"
      "  --cmake-cpp-standard <17|20|23>        C++ standard for CMake\n"
      "  --cmake-target-type <executable|...>   CMake target type\n"
      "  --cmake-enable-testing                 Enable testing in CMake\n"
      "  --cmake-enable-sanitizers              Enable sanitizers in CMake\n"
      "  --generate-src                         Generate src/main.cpp\n"
      "  --interactive, -i                      Run interactive wizard\n");

  app.set_subcommand_help(
      "install",
      "Usage:\n"
      "  metis install\n\n"
      "Installs pre-commit hook and optionally CI workflows based on .metis.toml.\n\n"
      "Configuration (.metis.toml):\n"
      "  [output]\n"
      "    local_hook     = true       # Install .git/hooks/pre-commit\n"
      "    github_actions = false      # Generate GitHub Actions workflow\n"
      "    gitlab_ci      = false      # Generate GitLab CI workflow\n");

  app.set_subcommand_help("deps",
                          "Usage:\n"
                          "  metis deps [OPTIONS]\n\n"
                          "Inspect:\n"
                          "  --verbose, -V            Show detailed output\n"
                          "  --graph, -g              Generate dependency graph\n"
                          "  --tree, -t               Display dependency tree\n"
                          "  --check-updates, -u      Check for newer versions\n\n"
                          "Manage:\n"
                          "  --add <name> <version>   Add a dependency\n"
                          "  --remove <name>          Remove a dependency\n"
                          "  --update <name> <version>  Change a dependency's version\n"
                          "  --source <conan|vcpkg|cmake>  Restrict to one manifest\n"
                          "  --yes, -y                Skip confirmation prompts\n");

  app.set_subcommand_help("install-compiler",
                          "Usage:\n"
                          "  metis install-compiler [OPTIONS]\n\n"
                          "Options:\n"
                          "  --compiler <gcc|clang>         Compiler to install [default: gcc]\n"
                          "  --version <version>            Specific version to install\n"
                          "  --cpp-standard <17|20|23|26>   C++ standard [default: 20]\n"
                          "  --prefix <path>                Installation prefix\n"
                          "  --force                        Reinstall if already present\n"
                          "  --dry-run, -n                  Show what would be installed\n");

  app.set_subcommand_help("generate-gha",
                          "Usage:\n"
                          "  metis generate-gha\n\n"
                          "Generates a GitHub Actions workflow at .github/workflows/metis.yml\n");

  app.set_subcommand_help("generate-gitlab",
                          "Usage:\n"
                          "  metis generate-gitlab\n\n"
                          "Generates a GitLab CI workflow at .gitlab-ci.yml\n");

  app.set_subcommand_help("sync",
                          "Usage:\n"
                          "  metis sync\n\n"
                          "Refreshes hooks/workflows and validates every enabled check.\n"
                          "Self-heals missing tooling configs without overwriting user files.\n");

  if (!app.parse(argc, argv)) {
    return 0;
  }

  try {
    auto subcmd = app.get_subcommand();

    if (subcmd == "init") {
      auto fs = std::make_unique<infrastructure::OsFileSystem>();
      auto config_repo = std::make_unique<infrastructure::TomlConfigRepository>(
          std::make_unique<infrastructure::OsFileSystem>(),
          std::make_unique<infrastructure::ProcessShellExecutor>());

      application::InitOptions opts;
      opts.project_name = fs->current_path().filename().string();

      bool interactive = false;
      for (size_t i = 1; i < args.size(); ++i) {
        std::string_view arg = args[i];
        if (arg == "--interactive" || arg == "-i") {
          interactive = true;
          break;
        }
      }

      bool has_flags = false;
      for (size_t i = 1; i < args.size(); ++i) {
        std::string_view arg = args[i];
        if (arg.starts_with("--") || arg.starts_with('-')) {
          has_flags = true;
          break;
        }
      }

      if (interactive || !has_flags) {
        presentation::run_interactive_init(opts);
      } else {
        if (!parse_init_flags(args, opts)) {
          Console::print_error_block("Invalid arguments", "Run metis init --help for usage");
          return static_cast<int>(domain::ExitCode::INVALID_ARGUMENTS);
        }
      }

      auto cwd = fs->current_path();
      application::InitUseCase init_use_case(std::move(config_repo), std::move(fs));
      auto result = init_use_case.execute(cwd, opts);

      if (!result.success) {
        Console::print_error_block(result.error_message, "Check permissions and try again");
        return static_cast<int>(domain::ExitCode::GENERAL_ERROR);
      }

      presentation::print_init_summary(opts, result);
      return static_cast<int>(domain::ExitCode::SUCCESS);
    }

    // Build shared infrastructure for remaining subcommands
    auto shell = std::make_unique<infrastructure::ProcessShellExecutor>();
    auto fs = std::make_unique<infrastructure::OsFileSystem>();
    auto git_repo = std::make_unique<infrastructure::CliGitRepository>(
        std::make_unique<infrastructure::ProcessShellExecutor>());
    auto config_repo = std::make_unique<infrastructure::TomlConfigRepository>(
        std::make_unique<infrastructure::OsFileSystem>(),
        std::make_unique<infrastructure::ProcessShellExecutor>());

    domain::config::ProjectConfig cfg;
    std::filesystem::path repo_root;

    try {
      cfg = config_repo->load(config_path);
      repo_root = config_repo->find_git_root();
    } catch (const std::exception& e) {
      Console::print_error_block(
          e.what(), "Run \" metis init \" to create a config, or specify one with --config");
      return static_cast<int>(domain::ExitCode::CONFIG_ERROR);
    }

    if (subcmd == "install") {
      application::InstallUseCase install_use_case(std::move(fs), std::move(git_repo));
      auto result = install_use_case.execute(repo_root, cfg);

      if (!result.error_message.empty()) {
        Console::print_error_block(result.error_message);
        return static_cast<int>(domain::ExitCode::HOOK_INSTALL_ERROR);
      }

      if (result.hook_installed) {
        Console::print_success_block("Pre-commit hook installed");
        Console::print_bullet("Location: " + result.hook_path);
      }
      if (result.workflow_installed) {
        Console::print_success_block("CI workflow installed");
        Console::print_bullet("Location: " + result.workflow_path);
      }

      Console::print_next_steps({
          "Stage files: " + Console::bold("git add ."),
          "Commit to trigger checks: " + Console::bold("git commit -m <message>"),
      });
      return static_cast<int>(domain::ExitCode::SUCCESS);
    }

    if (subcmd == "sync") {
      // Refresh hooks/workflows per [output] config (idempotent), then
      // validate every enabled check's tool/config presence.
      application::InstallUseCase install_use_case(std::move(fs), std::move(git_repo));
      auto installed = install_use_case.execute(repo_root, cfg);
      if (!installed.error_message.empty()) {
        Console::print_error_block(installed.error_message);
        return static_cast<int>(domain::ExitCode::HOOK_INSTALL_ERROR);
      }
      if (installed.hook_installed) {
        Console::print_success_block("Pre-commit hook synced");
        Console::print_bullet("Location: " + installed.hook_path);
      }
      if (installed.workflow_installed) {
        Console::print_success_block("CI workflow synced");
        Console::print_bullet("Location: " + installed.workflow_path);
      }

      // Self-heal: generate missing tooling configs from init defaults.
      // Never overwrites existing files — those are user-owned by now.
      namespace gen = metis::generators;
      auto sync_fs = std::make_unique<infrastructure::OsFileSystem>();
      const auto ensure_file = [&](const std::filesystem::path& path, const std::string& content) {
        if (sync_fs->exists(path)) {
          return;
        }
        if (!sync_fs->write_file(path, content)) {
          Console::print_error_block("Failed to write " + path.string());
          std::exit(static_cast<int>(domain::ExitCode::GENERAL_ERROR));
        }
        Console::print_success_block("Generated " + path.string());
      };

      int problems = 0;
      for (const auto& check_cfg : cfg.checks) {
        if (!check_cfg.enabled) {
          continue;
        }
        const auto base = std::filesystem::path(check_cfg.command).filename().string();
        if (base == "clang-format") {
          ensure_file(repo_root / ".clang-format",
                      gen::generate_clang_format("Google", 2, 100, "Left", "Attach"));
        } else if (base == "clang-tidy") {
          ensure_file(repo_root / ".clang-tidy", gen::generate_clang_tidy("standard", "error", 1));
        }

        auto impl = application::make_check(check_cfg);
        auto error = impl->validate(repo_root);
        if (!error.empty()) {
          Console::print_warning_block(error);
          ++problems;
        }
      }

      if (problems == 0) {
        Console::print_success_block("Environment is in sync");
        return static_cast<int>(domain::ExitCode::SUCCESS);
      }
      std::cerr << "[ERROR] " << problems << " check(s) need attention\n";
      return static_cast<int>(domain::ExitCode::CONFIG_ERROR);
    }

    if (subcmd == "generate-gha" || subcmd == "generate-gitlab") {
      auto platform = subcmd == "generate-gha" ? domain::workflow::Platform::GithubAction
                                               : domain::workflow::Platform::GitLabCI;
      auto label = subcmd == "generate-gha" ? "GitHub Actions" : "GitLab CI";
      application::GenerateWorkflowUseCase gen_use_case(std::move(fs));
      if (!gen_use_case.execute(cfg, repo_root, platform)) {
        Console::print_error_block("Failed to write " + std::string(label) + " workflow");
        return static_cast<int>(domain::ExitCode::WORKFLOW_GENERATION_ERROR);
      }
      Console::print_success_block(std::string(label) + " workflow generated");
      auto path = platform == domain::workflow::Platform::GithubAction
                      ? repo_root / ".github" / "workflows" / "metis.yml"
                      : repo_root / ".gitlab-ci.yml";
      Console::print_bullet(path.string());
      return static_cast<int>(domain::ExitCode::SUCCESS);
    }

    if (subcmd == "test") {
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

      if (!build_dir_override.empty()) {
        cfg.test.build_dir = build_dir_override;
      }

      auto test_use_case = application::TestChecksUseCase(std::move(shell), std::move(fs));
      auto start = std::chrono::steady_clock::now();
      auto result = test_use_case.execute(cfg, repo_root, coverage, verbose);
      auto end = std::chrono::steady_clock::now();
      std::chrono::duration<double> elapsed = end - start;

      if (!result.output.empty()) {
        std::cout << result.output;
      }

      SummaryReporter::TestSummary summary;
      summary.success = result.success;
      summary.total_tests = result.total_count;
      summary.failed_tests = result.failed_count;
      summary.line_coverage = result.line_coverage;
      summary.branch_coverage = result.branch_coverage;
      summary.function_coverage = result.function_coverage;
      summary.coverage_enabled = coverage;
      summary.coverage_ok = result.coverage_ok;
      SummaryReporter::print_test_summary(summary);

      if (!result.success) {
        return static_cast<int>(domain::ExitCode::TEST_FAILURE);
      }
      if (coverage && !result.coverage_ok) {
        return static_cast<int>(domain::ExitCode::COVERAGE_THRESHOLD_NOT_MET);
      }
      return static_cast<int>(domain::ExitCode::SUCCESS);
    }

    if (subcmd == "sanitizer") {
      bool verbose = false;
      for (size_t i = 1; i < args.size(); ++i) {
        std::string_view arg = args[i];
        if (arg == "sanitizer") {
          continue;
        }
        if (arg == "--verbose" || arg == "-V") {
          verbose = true;
        }
      }

      if (!cfg.sanitizer.enabled) {
        Console::print_warning_block("Sanitizers not enabled in config");
        Console::print_hint_block(
            "Add a [sanitizers] section to .metis.toml or run metis init --enable-sanitizers");
        return static_cast<int>(domain::ExitCode::SUCCESS);
      }

      auto sanitizer_use_case =
          application::SanitizerChecksUseCase(std::move(shell), std::move(fs));
      bool ok = sanitizer_use_case.execute(cfg, repo_root, verbose);

      SummaryReporter::PhaseSummary summary;
      summary.phase_name = "Sanitizer checks";
      summary.success = ok;
      for (const auto& type : cfg.sanitizer.types) {
        summary.details.push_back(type);
      }
      SummaryReporter::print_phase_summary(summary);

      if (!ok) {
        return static_cast<int>(domain::ExitCode::SANITIZER_TEST_FAILURE);
      }
      return static_cast<int>(domain::ExitCode::SUCCESS);
    }

    if (subcmd == "perf") {
      bool verbose = false;
      auto level = application::PerfLevel::FULL;
      // third level exists.
      for (size_t i = 1; i < args.size(); ++i) {
        std::string_view arg = args[i];
        if (arg == "perf") {
          continue;
        }
        if (arg == "--verbose" || arg == "-V") {
          verbose = true;
        }
        if (arg == "--level" && i + 1 < args.size()) {
          level = std::string_view(args[++i]) == "quick" ? application::PerfLevel::QUICK
                                                         : application::PerfLevel::FULL;
        }
      }

      if (!cfg.perf.enabled) {
        Console::print_warning_block("Performance checks not enabled in config");
        Console::print_hint_block("Add a [perf] section to .metis.toml");
        return static_cast<int>(domain::ExitCode::SUCCESS);
      }

      auto perf_use_case = application::PerfChecksUseCase(std::move(shell), std::move(fs));
      auto result = perf_use_case.execute(cfg, repo_root, verbose, level);

      if (!result.output.empty()) {
        std::cout << result.output;
      }

      SummaryReporter::PhaseSummary summary;
      summary.phase_name = "Performance checks";
      summary.success = result.success;
      if (cfg.perf.max_build_time_sec > 0 && result.build_time_sec > 0) {
        summary.details.push_back(fmt::format("Build time: {:.1f}s", result.build_time_sec));
      }
      if (cfg.perf.max_binary_size_mb > 0) {
        summary.details.push_back(fmt::format("Binary size: {} bytes", result.binary_size_bytes));
      }
      SummaryReporter::print_phase_summary(summary);

      if (!result.success) {
        return static_cast<int>(domain::ExitCode::PERF_CHECK_FAILURE);
      }
      return static_cast<int>(domain::ExitCode::SUCCESS);
    }

    if (subcmd == "run") {
      bool all_files = false;
      bool diff_only = false;
      bool verbose = false;
      bool dry_run = false;
      bool format_mode = false;
      bool no_cache = false;
      std::vector<std::string> run_files;

      for (size_t i = 1; i < args.size(); ++i) {
        std::string_view arg = args[i];
        if (arg == "run") {
          continue;
        }
        if (arg == "--all-files") {
          all_files = true;
        } else if (arg == "--diff-only") {
          diff_only = true;
        } else if (arg == "--verbose" || arg == "-V" || arg == "--detail") {
          verbose = true;
        } else if (arg == "--dry-run" || arg == "-n") {
          dry_run = true;
        } else if (arg == "--format" || arg == "-f") {
          format_mode = true;
        } else if (arg == "--no-cache") {
          no_cache = true;
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
      } else if (diff_only) {
        opts.source = application::FileSource::STAGED;
      } else if (!run_files.empty()) {
        opts.source = application::FileSource::EXPLICIT;
      } else {
        opts.source = application::FileSource::STAGED;
      }

      if (!run_files.empty()) {
        opts.explicit_files = std::move(run_files);
      }

      application::RunChecksUseCase run_use_case(std::move(shell), std::move(git_repo),
                                                 std::move(fs));

      // The cache is only useful for a diff (staged/explicit files); a
      // full-codebase sweep re-checks everything, so skip the cache there.
      // check_cache must outlive execute() so cache_ isn't dangling.
      std::unique_ptr<infrastructure::CheckCache> check_cache;
      if (opts.source != application::FileSource::ALL_REPO && !no_cache) {
        check_cache = std::make_unique<infrastructure::CheckCache>(repo_root);
        run_use_case.set_cache(check_cache.get());
      }

      auto start = std::chrono::steady_clock::now();
      int exit_code = run_use_case.execute(cfg, opts);
      auto end = std::chrono::steady_clock::now();
      std::chrono::duration<double> elapsed = end - start;

      // Note: RunChecksUseCase handles its own output. We add a summary footer here.
      // YAGNI: The use case would need to return a summary struct for a proper report.
      // For now, we print a simple footer based on exit code.
      if (exit_code == 0) {
        Console::print_success_block("All checks passed");
      } else {
        Console::print_error_block(
            "One or more checks failed",
            "Run metis run --verbose for details, or git commit --no-verify to skip");
      }

      if (opts.verbose) {
        Console::print_success_block(
            fmt::format("[metis] [INFO] completed in {:.2f}s\n", elapsed.count()));
      }

      return exit_code;
    }

    if (subcmd == "install-compiler") {
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

      domain::ports::CppStandard cpp_standard = domain::ports::CppStandard::CPP_20;
      if (cpp_standard_str == "17") {
        cpp_standard = domain::ports::CppStandard::CPP_17;
      } else if (cpp_standard_str == "20") {
        cpp_standard = domain::ports::CppStandard::CPP_20;
      } else if (cpp_standard_str == "23") {
        cpp_standard = domain::ports::CppStandard::CPP_23;
      } else if (cpp_standard_str == "26") {
        cpp_standard = domain::ports::CppStandard::CPP_26;
      } else {
        Console::print_error_block("Invalid --cpp-standard", "Use 17, 20, 23, or 26");
        return static_cast<int>(domain::ExitCode::INVALID_ARGUMENTS);
      }

      auto provider =
          infrastructure::ToolchainFactory::create(compiler, version, shell.get(), fs.get());

      if (!provider) {
        Console::print_error_block(compiler + " installation is not supported on this platform");
        return static_cast<int>(domain::ExitCode::UNSUPPORTED_PLATFORM);
      }

      if (!provider->supports_cpp_standard(cpp_standard)) {
        auto max_std = provider->max_supported_standard();
        Console::print_error_block(
            fmt::format("C++{} is not supported by this compiler", cpp_standard_str),
            fmt::format("Maximum supported: C++{}", static_cast<int>(max_std)));
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
        Console::print_info_block(result.error_message_);
        return static_cast<int>(domain::ExitCode::SUCCESS);
      }

      if (!result.success_) {
        Console::print_error_block(result.error_message_);
        return static_cast<int>(domain::ExitCode::TOOLCHAIN_INSTALL_ERROR);
      }

      Console::print_success_block(fmt::format("{} {} installed", compiler, result.version_));
      Console::print_bullet(
          fmt::format("C++ standard: {}", static_cast<int>(result.installed_cpp_standard_)));
      Console::print_bullet(fmt::format("Path: {}", result.installed_path_));
      return static_cast<int>(domain::ExitCode::SUCCESS);
    }

    if (subcmd == "build") {
      bool verbose = false;
      bool clean = false;
      std::string build_dir = "build";
      int jobs = 0;

      for (size_t i = 1; i < args.size(); ++i) {
        std::string_view arg = args[i];

        if (arg == "build") {
          continue;
        }

        if (arg == "--verbose" || arg == "-V") {
          verbose = true;
        } else if (arg == "--clean") {
          clean = true;
        } else if (arg == "--build-dir" && i + 1 < args.size()) {
          build_dir = args[++i];
        } else if ((arg == "-j" || arg == "--jobs") && i + 1 < args.size()) {
          jobs = std::stoi(args[++i]);
        }
      }

      auto build_uc =
          application::BuildUseCase(std::make_unique<infrastructure::ProcessShellExecutor>(),
                                    std::make_unique<infrastructure::OsFileSystem>());

      auto result = build_uc.execute(repo_root, build_dir, clean, verbose, jobs);

      if (!result.output.empty()) {
        std::cout << result.output;
      }

      SummaryReporter::PhaseSummary summary;
      summary.phase_name = "Build";
      summary.success = result.success;
      summary.details.push_back(fmt::format("Configure: {:.1f}s", result.configure_time_sec));
      summary.details.push_back(fmt::format("Build:     {:.1f}s", result.build_time_sec));

      SummaryReporter::print_phase_summary(summary);

      if (!result.success) {
        return static_cast<int>(domain::ExitCode::BUILD_FAILURE);
      }

      return static_cast<int>(domain::ExitCode::SUCCESS);
    }

    if (subcmd == "deps") {
      application::DependencyCheckOptions dep_opts;
      application::DependencyManageOptions manage_opts;
      bool do_manage = false;

      for (size_t i = 2; i < args.size(); ++i) {
        std::string_view arg = args[i];
        if (arg == "--verbose" || arg == "-V") {
          dep_opts.verbose = true;
        } else if (arg == "--graph" || arg == "-g") {
          dep_opts.generate_graph = true;
        } else if (arg == "--tree" || arg == "-t") {
          dep_opts.display_tree = true;
        } else if (arg == "--check-updates" || arg == "-u") {
          dep_opts.check_updates = true;
        } else if (arg == "--update") {
          do_manage = true;
          manage_opts.action = application::DependencyManageOptions::Action::UPDATE;
          if (i + 1 < args.size() && !std::string_view(args[i + 1]).starts_with("-")) {
            manage_opts.dependency_name = args[++i];
          }
          if (i + 1 < args.size() && !std::string_view(args[i + 1]).starts_with("-")) {
            manage_opts.version = args[++i];
          }
        } else if (arg == "--remove") {
          do_manage = true;
          manage_opts.action = application::DependencyManageOptions::Action::REMOVE;
          if (i + 1 < args.size() && !std::string_view(args[i + 1]).starts_with("-")) {
            manage_opts.dependency_name = args[++i];
          }
        } else if (arg == "--add") {
          do_manage = true;
          manage_opts.action = application::DependencyManageOptions::Action::ADD;
          if (i + 1 < args.size() && !std::string_view(args[i + 1]).starts_with("-")) {
            manage_opts.dependency_name = args[++i];
          }
          if (i + 1 < args.size() && !std::string_view(args[i + 1]).starts_with("-")) {
            manage_opts.version = args[++i];
          }
        } else if (arg == "--source" && i + 1 < args.size()) {
          manage_opts.source = args[++i];
        } else if (arg == "--yes" || arg == "-y") {
          manage_opts.yes = true;
        }
      }

      auto deps_fs = std::make_unique<infrastructure::OsFileSystem>();
      auto deps_shell = std::make_unique<infrastructure::ProcessShellExecutor>();
      auto deps_http = std::make_unique<infrastructure::CurlHttpClient>(deps_shell.get());
      domain::ports::IFileSystem* fs_ptr = deps_fs.get();
      domain::ports::IShellExecutor* shell_ptr = deps_shell.get();

      if (do_manage) {
        if (manage_opts.dependency_name.empty()) {
          Console::print_error_block("Missing dependency name",
                                     "Usage: metis deps --remove <name>");
          return static_cast<int>(domain::ExitCode::INVALID_ARGUMENTS);
        }

        application::DependencyManageUseCase manage_uc(std::move(deps_fs));
        manage_uc.register_editor(
            std::make_unique<infrastructure::ConanManifestEditor>(fs_ptr));
        manage_uc.register_editor(
            std::make_unique<infrastructure::VcpkgManifestEditor>(fs_ptr));
        manage_uc.register_editor(
            std::make_unique<infrastructure::CMakeManifestEditor>(fs_ptr));

        auto result = manage_uc.execute(repo_root, manage_opts);

        for (const auto& msg : result.messages) {
          if (result.success) {
            Console::print_success_block(msg);
          } else {
            Console::print_error_block(msg);
          }
        }

        if (!result.modified_files.empty()) {
          Console::print_info_block("Modified manifests:");
          for (const auto& f : result.modified_files) {
            Console::print_bullet(f, 4);
          }
        }

        return static_cast<int>(result.success ? domain::ExitCode::SUCCESS
                                               : domain::ExitCode::GENERAL_ERROR);
      }

      application::DependencyCheckUseCase deps_uc(std::move(deps_shell), std::move(deps_fs));
      deps_uc.register_parser(
          std::make_unique<infrastructure::ConanDependencyParser>(fs_ptr));
      deps_uc.register_parser(
          std::make_unique<infrastructure::VcpkgDependencyParser>(fs_ptr));
      deps_uc.register_parser(
          std::make_unique<infrastructure::CMakeDependencyParser>(fs_ptr));
      deps_uc.register_version_checker(
          std::make_unique<infrastructure::ConanVersionChecker>(shell_ptr));
      deps_uc.register_version_checker(
          std::make_unique<infrastructure::VcpkgVersionChecker>(shell_ptr));
      deps_uc.register_version_checker(std::make_unique<infrastructure::CMakeVersionChecker>(
          deps_http.get()));

      auto result = deps_uc.execute(repo_root, dep_opts);

      if (!dep_opts.display_tree) {
        Console::print_header("Dependencies");

        if (result.validations.empty()) {
          Console::print_info_block(
              "No dependency manifest found (conanfile.py, vcpkg.json, CMakeLists.txt)");
        } else {
          size_t name_width = 0;
          for (const auto& res_validation : result.validations) {
            name_width = std::max(name_width, res_validation.dep.name.size());
          }

          for (const auto& res_validation : result.validations) {
            std::cout << "  " << (res_validation.ok ? Console::green("✓") : Console::red("✖"))
                      << " " << res_validation.dep.name
                      << std::string(name_width - res_validation.dep.name.size(), ' ');

            if (!res_validation.dep.version.empty()) {
              std::cout << "  " << Console::dim(res_validation.dep.version);
            }

            if (res_validation.dep.has_update && res_validation.dep.latest_version.has_value()) {
              std::cout << "  " << Console::cyan("→ " + res_validation.dep.latest_version.value());
            }

            if (!res_validation.ok) {
              std::cout << "  " << Console::red("[" + res_validation.message + "]");
            } else if (dep_opts.verbose) {
              std::cout << "  " << Console::dim("(" + res_validation.dep.source + ")");
            }

            std::cout << "\n";
          }
        }
      }

      if (!result.duplicates.empty()) {
        Console::print_warning_block("Duplicate dependencies detected");
        for (const auto& dup : result.duplicates) {
          Console::print_bullet(dup, 4);
        }
      }

      if (!result.lockfile_issues.empty()) {
        Console::print_warning_block("Lockfile issues");
        for (const auto& issue : result.lockfile_issues) {
          Console::print_bullet(issue, 4);
        }
      }

      if (dep_opts.generate_graph && !result.validations.empty()) {
        Console::print_info_block("Dependency graph written to " + dep_opts.graph_output_path);
      }

      if (dep_opts.check_updates) {
        SummaryReporter::DepUpdateSummary update_summary;
        update_summary.outdated = result.outdated;
        SummaryReporter::print_dep_updates(update_summary);
      }

      SummaryReporter::DepSummary summary;
      summary.total = static_cast<int>(result.validations.size());
      summary.valid = static_cast<int>(
          std::ranges::count_if(result.validations, [](const auto& v) { return v.ok; }));
      summary.invalid = summary.total - summary.valid;
      summary.duplicates = static_cast<int>(result.duplicates.size());
      summary.lockfile_issues = static_cast<int>(result.lockfile_issues.size());
      summary.outdated = static_cast<int>(result.outdated.size());
      SummaryReporter::print_dep_summary(summary);

      return static_cast<int>(result.success() ? domain::ExitCode::SUCCESS
                                               : domain::ExitCode::MISSING_DEPENDENCY);
    }

    app.show_help();
    Console::print_hint_block("Run metis <command> --help for detailed usage");
    return static_cast<int>(domain::ExitCode::INVALID_ARGUMENTS);

  } catch (const std::exception& error) {
    Console::print_error_block(error.what(),
                               "If this persists, file an issue with --verbose output");
    return static_cast<int>(domain::ExitCode::GENERAL_ERROR);
  } catch (...) {
    Console::print_error_block("Unknown fatal error occurred", "Please report this bug");
    return static_cast<int>(domain::ExitCode::GENERAL_ERROR);
  }
}
