#include <fmt/format.h>

#include <cstring>
#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "sniffercommit/application/generate_workflow_use_case.hpp"
#include "sniffercommit/application/init_use_case.hpp"
#include "sniffercommit/application/install_toolchain_use_case.hpp"
#include "sniffercommit/application/install_use_case.hpp"
#include "sniffercommit/application/run_checks_use_case.hpp"
#include "sniffercommit/application/sanitizer_checks_use_case.hpp"
#include "sniffercommit/application/test_checks_use_case.hpp"
#include "sniffercommit/argparse.hpp"
#include "sniffercommit/domain/config.hpp"
#include "sniffercommit/domain/error_codes.hpp"
#include "sniffercommit/domain/ports/toolchain_provider.hpp"
#include "sniffercommit/domain/workflow.hpp"
#include "sniffercommit/infrastructure/cli_git_repository.hpp"
#include "sniffercommit/infrastructure/os_file_system.hpp"
#include "sniffercommit/infrastructure/process_shell_executor.hpp"
#include "sniffercommit/infrastructure/toml_config_repository.hpp"
#include "sniffercommit/infrastructure/toolchain_factory.hpp"
#ifdef _WIN32
#include "sniffercommit/infrastructure/zip_archive_extractor.hpp"
#endif  // _WIN32
#include "sniffercommit/domain/ports/archive_extractor.hpp"
#include "sniffercommit/infrastructure/curl_http_client.hpp"
#include "sniffercommit/infrastructure/tar_archive_extractor.hpp"
#include "sniffercommit/presentation/interactive_init.hpp"

namespace {

// Pre-parses --config/-c from argv before ArgParser runs.
// This is needed because the config path must be known before loading
// the config file, but ArgParser processes it as a regular option.
// lazy: duplicated parsing — ArgParser could support pre-parse hooks,
// but that's more code for a one-off need.
std::string preparse_config_path(std::span<char*> args) {
  std::string config_path = ".sniffercommit.toml";
  for (size_t i = 1; i + 1 < args.size(); ++i) {
    std::string_view arg = args[i];
    if ((arg == "-c" || arg == "--config") && i + 1 < args.size()) {
      config_path = args[i + 1];
      break;
    }
  }
  return config_path;
}

// Manually parses init-specific flags from argv.
// lazy: ArgParser handles subcommands but not value-bearing options for init.
// This is a second parser that runs after ArgParser identifies the subcommand.
// The two parsers could be unified, but ArgParser's add_option template
// doesn't support all the init flags cleanly.
bool parse_init_flags(std::span<char*> args, sniffercommit::application::InitOptions& opts) {
  auto to_lower = [](std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
  };

  auto safe_stoi = [](const char* str, int& out) -> bool {
    try {
      size_t pos = 0;
      int val = std::stoi(str, &pos);
      if (pos != std::strlen(str)) return false;
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
    }
  }
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  using namespace sniffercommit;

  auto argc_sz = static_cast<size_t>(argc);
  std::span<char*> args(argv, argc_sz);
  std::string config_path = preparse_config_path(args);

  ArgParser app("sniffercommit", "Fast C++20-powered pre-commit & CI generator");
  app.set_version("0.4.0")
      .add_option("-c", "--config", "Config file path", config_path)
      .add_subcommand("init", "Create default .sniffercommit.toml")
      .add_subcommand("install", "Generate & install .git/hooks/pre-commit")
      .add_subcommand("generate-gha", "Output GitHub Actions workflow")
      .add_subcommand("generate-gitlab", "Output GitLab CI workflow")
      .add_subcommand("run", "Execute checks on files")
      .add_subcommand("install-compiler", "Download and install a C++ toolchain")
      .add_subcommand("sanitizer", "Run sanitizer checks (ASan, UBSan, TSan, LSan)")
      .add_subcommand("test", "Run test and optional coverage checks");

  if (!app.parse(argc, argv)) {
    return 0;
  }

  try {
    auto subcmd = app.get_subcommand();

    if (subcmd == "init") {
      // Init needs its own filesystem and config repo instances because
      // it creates files (config, .clang-format, etc.) from templates.
      auto fs = std::make_unique<infrastructure::OsFileSystem>();
      auto config_repo = std::make_unique<infrastructure::TomlConfigRepository>(
          std::make_unique<infrastructure::OsFileSystem>(),
          std::make_unique<infrastructure::ProcessShellExecutor>());

      application::InitOptions opts;
      opts.project_name = fs->current_path().filename().string();

      // Determine if we should run interactive mode:
      //   - Explicit --interactive/-i flag → interactive
      //   - No flags at all → interactive (sensible default for new users)
      //   - Any flags present → CLI-only mode
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
          return static_cast<int>(domain::ExitCode::INVALID_ARGUMENTS);
        }
      }

      auto cwd = fs->current_path();
      application::InitUseCase init_use_case(std::move(config_repo), std::move(fs));
      auto result = init_use_case.execute(cwd, opts);

      if (!result.success) {
        std::cerr << "[ERROR] " << result.error_message << "\n";
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

    domain::config::ProjectConfig cfg = config_repo->load(config_path);
    auto repo_root = config_repo->find_git_root();

    if (subcmd == "install") {
      application::InstallUseCase install_use_case(std::move(fs), std::move(git_repo));
      auto result = install_use_case.execute(repo_root, cfg);

      if (!result.error_message.empty()) {
        std::cerr << "[ERROR] " << result.error_message << "\n";
        return static_cast<int>(domain::ExitCode::HOOK_INSTALL_ERROR);
      }

      if (result.hook_installed) {
        std::cout << "[INFO] pre-commit hook installed at " << result.hook_path << "\n";
      }
      if (result.workflow_installed) {
        std::cout << "[INFO] workflow installed at " << result.workflow_path << "\n";
      }
      return static_cast<int>(domain::ExitCode::SUCCESS);
    }

    if (subcmd == "generate-gha") {
      application::GenerateWorkflowUseCase gen_use_case(std::move(fs));
      if (!gen_use_case.execute(cfg, repo_root)) {
        std::cerr << "[ERROR] Failed to write GitHub Actions workflow\n";
        return static_cast<int>(domain::ExitCode::WORKFLOW_GENERATION_ERROR);
      }
      std::cout << "[INFO] GitHub Actions workflow generated at "
                << (repo_root / ".github" / "workflows" / "sniffercommit.yml").string() << "\n";
      return static_cast<int>(domain::ExitCode::SUCCESS);
    }

    if (subcmd == "generate-gitlab") {
      application::GenerateWorkflowUseCase gen_use_case(std::move(fs));
      if (!gen_use_case.execute(cfg, repo_root, domain::workflow::Platform::GitLabCI)) {
        std::cerr << "[ERROR] Failed to write GitLab CI workflow\n";
        return static_cast<int>(domain::ExitCode::WORKFLOW_GENERATION_ERROR);
      }
      std::cout << "[INFO] GitLab CI workflow generated at "
                << (repo_root / ".gitlab-ci.yml").string() << "\n";
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

      auto result = test_use_case.execute(cfg, repo_root, coverage, verbose);

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
        std::cerr << "[WARN] Sanitizers not enabled in config.\n";
        return static_cast<int>(domain::ExitCode::SUCCESS);
      }

      domain::config::ProjectConfig cfg_data = config_repo->load(config_path);

      auto sanitizer_use_case =
          application::SanitizerChecksUseCase(std::move(shell), std::move(fs));
      bool ok = sanitizer_use_case.execute(cfg_data, repo_root, verbose);

      if (!ok) {
        return static_cast<int>(domain::ExitCode::SANITIZER_TEST_FAILURE);
      }

      return static_cast<int>(domain::ExitCode::SUCCESS);
    }

    if (subcmd == "run") {
      // Run subcommand: executes checks against files.
      // Parses its own flags (--all-files, --verbose, --dry-run, --format)
      // and builds a RunOptions struct for the use case.
      bool all_files = false;
      bool verbose = false;
      bool dry_run = false;
      bool format_mode = false;
      std::vector<std::string> run_files;

      for (size_t i = 1; i < args.size(); ++i) {
        std::string_view arg = args[i];
        if (arg == "run") continue;
        if (arg == "--all-files")
          all_files = true;
        else if (arg == "--verbose" || arg == "-V" || arg == "--detail")
          verbose = true;
        else if (arg == "--dry-run" || arg == "-n")
          dry_run = true;
        else if (arg == "--format" || arg == "-f")
          format_mode = true;
        else if (!arg.starts_with('-'))
          run_files.emplace_back(arg);
      }

      application::RunOptions opts;
      opts.verbose = verbose;
      opts.dry_run = dry_run;
      opts.mode = format_mode ? application::RunMode::FORMAT : application::RunMode::CHECK;

      if (all_files)
        opts.source = application::FileSource::ALL_REPO;
      else if (!run_files.empty())
        opts.source = application::FileSource::EXPLICIT;
      else
        opts.source = application::FileSource::STAGED;

      if (!run_files.empty()) opts.explicit_files = std::move(run_files);

      application::RunChecksUseCase run_use_case(std::move(shell), std::move(git_repo),
                                                 std::move(fs));
      return run_use_case.execute(cfg, opts);
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
        std::cerr << "[ERROR] Invalid --cpp-standard. use 17, 20, or 23\n";
        return static_cast<int>(domain::ExitCode::INVALID_ARGUMENTS);
      }

      auto provider =
          infrastructure::ToolchainFactory::create(compiler, version, shell.get(), fs.get());

      if (!provider) {
        std::cerr << "[ERROR] GCC installation is not supported on this platform\n";
        return static_cast<int>(domain::ExitCode::UNSUPPORTED_PLATFORM);
      }

      if (!provider->supports_cpp_standard(cpp_standard)) {
        auto max_std = provider->max_supported_standard();
        std::cerr << "[ERROR] C++ " << cpp_standard_str << " is not supported by this compiler "
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
                << static_cast<int>(result.installed_cpp_standard_) << ")" << " installed at "
                << result.installed_path_ << "\n";
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
