#include <fmt/format.h>

#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "sniffercommit/application/generate_workflow_use_case.hpp"
#include "sniffercommit/application/init_use_case.hpp"
#include "sniffercommit/application/install_use_case.hpp"
#include "sniffercommit/application/run_checks_use_case.hpp"
#include "sniffercommit/argparse.hpp"
#include "sniffercommit/domain/config.hpp"
#include "sniffercommit/domain/error_codes.hpp"
#include "sniffercommit/domain/workflow.hpp"
#include "sniffercommit/infrastructure/cli_git_repository.hpp"
#include "sniffercommit/infrastructure/os_file_system.hpp"
#include "sniffercommit/infrastructure/process_shell_executor.hpp"
#include "sniffercommit/infrastructure/toml_config_repository.hpp"
#include "sniffercommit/presentation/interactive_init.hpp"

namespace {

struct SafeArgs {
  std::span<char*> inner;

  char*& at(size_t i) {
    if (i >= inner.size()) {
      throw std::runtime_error("BUG: args index out of range");
    }
    return inner.data()[i];
  }
  size_t size() const { return inner.size(); }
};

std::string preparse_config_path(SafeArgs& args) {
  std::string config_path = ".sniffercommit.toml";
  for (size_t i = 1; i + 1 < args.size(); ++i) {
    std::string_view arg = args.at(i);
    if ((arg == "-c" || arg == "--config") && i + 1 < args.size()) {
      config_path = args.at(i + 1);
      break;
    }
  }
  return config_path;
}

bool parse_init_flags(SafeArgs& args, sniffercommit::application::InitOptions& opts) {
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
    std::string arg = args.at(i);
    if (arg == "--style" && i + 1 < args.size()) {
      opts.style = to_lower(args.at(++i));
    } else if (arg == "--name" && i + 1 < args.size()) {
      opts.project_name = args.at(++i);
    } else if (arg == "--indent-width" && i + 1 < args.size()) {
      safe_stoi(args.at(++i), opts.indent_width);
    } else if (arg == "--column-limit" && i + 1 < args.size()) {
      safe_stoi(args.at(++i), opts.column_limit);
    } else if (arg == "--pointer-alignment" && i + 1 < args.size()) {
      opts.pointer_alignment = args.at(++i);
    } else if (arg == "--brace-style" && i + 1 < args.size()) {
      opts.brace_style = args.at(++i);
    } else if (arg == "--enable-clang-tidy" || arg == "--tidy") {
      opts.enable_clang_tidy = true;
    } else if (arg == "--tidy-preset" && i + 1 < args.size()) {
      opts.tidy_preset = to_lower(args.at(++i));
    } else if (arg == "--tidy-severity" && i + 1 < args.size()) {
      opts.tidy_severity = to_lower(args.at(++i));
    } else if (arg == "--tidy-header-filter" && i + 1 < args.size()) {
      safe_stoi(args.at(++i), opts.tidy_header_filter);
    } else if (arg == "--enable-cmake" || arg == "--cmake") {
      opts.enable_cmake = true;
      opts.generate_source = true;
    } else if (arg == "--cmake-cpp-standard" && i + 1 < args.size()) {
      opts.cmake_cpp_standard = to_lower(args.at(++i));
    } else if (arg == "--cmake-target-type" && i + 1 < args.size()) {
      opts.cmake_target_type = to_lower(args.at(++i));
    } else if (arg == "--cmake-enable-testing") {
      opts.cmake_enable_testing = true;
    } else if (arg == "--cmake-enable-sanitizers") {
      opts.cmake_enable_sanitizers = true;
    } else if (arg == "--add-dep" && i + 1 < args.size()) {
      opts.dependencies.push_back(args.at(++i));
    } else if (arg == "--generate-src") {
      opts.generate_source = true;
    }
  }
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  using namespace sniffercommit;

  auto argc_sz = static_cast<size_t>(argc);
  std::span raw_args(argv, argc_sz);
  SafeArgs args{raw_args};
  std::string config_path = preparse_config_path(args);

  ArgParser app("sniffercommit", "Fast C++20-powered pre-commit & CI generator");
  app.set_version("0.3.9")
      .add_option("-c", "--config", "Config file path", config_path)
      .add_subcommand("init", "Create default .sniffercommit.toml")
      .add_subcommand("install", "Generate & install .git/hooks/pre-commit")
      .add_subcommand("generate-gha", "Output GitHub Actions workflow")
      .add_subcommand("run", "Execute checks on files");

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
        std::string_view arg = args.at(i);
        if (arg == "--interactive" || arg == "-i") {
          interactive = true;
          break;
        }
      }

      bool has_flags = false;
      for (size_t i = 1; i < args.size(); ++i) {
        std::string_view arg = args.at(i);
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

    if (subcmd == "run") {
      bool all_files = false;
      bool verbose = false;
      bool dry_run = false;
      bool format_mode = false;
      std::vector<std::string> run_files;

      for (size_t i = 1; i < args.size(); ++i) {
        std::string_view arg = args.at(i);
        if (arg == "run") continue;
        if (arg == "--all-files")
          all_files = true;
        else if (arg == "--verbose" || arg == "-V")
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
