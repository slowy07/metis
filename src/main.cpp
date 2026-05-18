#include <algorithm>
#include <cctype>
#include <cstring>
#include <exception>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include "fmt/format.h"
#include "sniffercommit/argparse.hpp"
#include "sniffercommit/cicd_domain.hpp"
#include "sniffercommit/config_manager.hpp"
#include "sniffercommit/executor.hpp"
#include "sniffercommit/precommit_domain.hpp"
#include "sniffercommit/project_config.hpp"
#include "sniffercommit/tooling_config.hpp"

namespace {

std::string preparse_config_path(int argc, char** argv) {
  std::string config_path = ".sniffercommit.toml";

  for (int i = 1; i < argc - 1; ++i) {
    std::string_view arg = argv[i];
    if ((arg == "-c" || arg == "--config") && i + 1 < argc) {
      config_path = argv[i + 1];
      break;
    }
  }
  return config_path;
}

template <typename T>
bool safe_stoi(const char* str, T& out) {
  try {
    size_t pos = 0;

    int val = std::stoi(str, &pos);
    if (pos != std::strlen(str)) {
      return false;
    }

    out = val;
    return true;
  } catch (const std::exception&) {
    return false;
  }
}

sniffercommit::tooling::TidyPreset parse_tidy_preset(const std::string& preset) {
  std::string lower;

  for (char c : preset) {
    lower += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }

  if (lower == "minimal") return sniffercommit::tooling::TidyPreset::Minimal;
  if (lower == "standard") return sniffercommit::tooling::TidyPreset::Standard;
  if (lower == "strict") return sniffercommit::tooling::TidyPreset::Strict;
  if (lower == "custom") return sniffercommit::tooling::TidyPreset::Custom;

  throw std::runtime_error("Unknown tidy preset: " + preset);
}

sniffercommit::tooling::TidySeverity parse_tidy_severity(const std::string& sev) {
  std::string lower;
  
  for (char c : sev) {
    lower += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }

  if (lower == "note") return sniffercommit::tooling::TidySeverity::Note;
  if (lower == "warning") return sniffercommit::tooling::TidySeverity::Warning;
  if (lower == "error") return sniffercommit::tooling::TidySeverity::Error;
  
  throw std::runtime_error("Unknown tidy severity: " + sev);
}

}  // namespace

int main(int argc, char** argv) {
  using namespace sniffercommit;

  std::string config_path = preparse_config_path(argc, argv);

  ArgParser app("sniffercommit", "Fast C++20-powered pre-commit & CI generator");
  app.set_version("0.2.1")
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
    ConfigManager manager;

    // INFO: init command
    if (subcmd == "init") {
      ConfigManager::InitOptions opts;
      opts.project_name = std::filesystem::current_path().filename().string();

      for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        // NOTE: stye command `--style`
        if (arg == "--style") {
          if (i + 1 >= argc) {
            std::cerr << "[ERROR] --style requires value\n";
            return 1;
          }

          std::string value = argv[++i];
          std::ranges::transform(value, value.begin(),
                                 [](unsigned char c) { return std::tolower(c); });

          try {
            opts.style = tooling::parse_style(value);
          } catch (const std::exception& error_style_config) {
            std::cerr << "[ERROR] " << error_style_config.what() << "\n";
            return 1;
          }
        }

        // NOTE: indent width command `--indent-width`
        if (arg == "--indent-width") {
          if (i + 1 >= argc || !safe_stoi(argv[++i], opts.indent_width)) {
            std::cerr << "[ERROR] --indent-width require integer value\n";
            return 1;
          }
        }

        // NOTE: column limit command `--column-limit`
        if (arg == "--column-limit") {
          if (i + 1 >= argc || !safe_stoi(argv[++i], opts.column_limit)) {
            std::cerr << "[ERROR] --column-limit requires integer value\n";
            return 1;
          }
        }

        // NOTE: pointer alignment command `--pointer-alignment`
        if (arg == "--pointer-alignment") {
          if (i + 1 >= argc) {
            std::cerr << "[ERROR] --pointer-alignment requires value\n";
            return 1;
          }
          opts.pointer_alignment = argv[++i];
        }

        // NOTE: brace style command `--brace-style`
        if (arg == "--brace-style") {
          if (i + 1 >= argc) {
            std::cerr << "[ERROR] --brace-style requires value\n";
            return 1;
          }
          opts.brace_style = argv[++i];
        }

        // NOTE: project name command `--name`
        if (arg == "--name") {
          if (i + 1 >= argc) {
            std::cerr << "[ERROR] --name-requires value\n";
            return 1;
          }

          opts.project_name = argv[++i];
        }

        // NOTE: clang tidy command "--enable-clang-tidy"
        if (arg == "--enable-clang-tidy" || arg == "--tidy") {
          opts.enable_clang_tidy = true;
        }

        if (arg == "--tidy-preset") {
          if (i + 1 >= argc) {
            std::cerr << "[ERROR] --tidy-preset requires value (minimal|standard|strict|custom)\n";
            return 1;
          }

          try {
            opts.tidy_preset = parse_tidy_preset(argv[++i]);
          } catch (const std::exception& error_tidy_preset) {
            std::cerr << "[ERROR] " << error_tidy_preset.what() << "\n";
            return 1;
          }
        }

        if (arg == "--tidy-severity") {
          if (i + 1 >= argc) {
            std::cerr << "[ERROR] --tidy-severity requires value (note|warning|error)\n";
            return 1;
          }

          try {
            opts.tidy_severity = parse_tidy_severity(argv[++i]);
          } catch (const std::exception& error_tidy_severity) {
            std::cerr << "[ERROR] " << error_tidy_severity.what() << "\n";
            return 1;
          }
        }

        if (arg == "--tidy-header-filter") {
          if (i + 1 >= argc || !safe_stoi(argv[++i], opts.tidy_header_filter)) {
            std::cerr << "[ERROR] --tidy-header-filter requires integer (0|1|2)\n";
            return 1;
          }

          if (opts.tidy_header_filter < 0 || opts.tidy_header_filter > 2) {
            std::cerr << "[ERROR] --tidy-header-filter must be 0, 1, or 2\n";
            return 1;
          }
        }
      }

      auto result = manager.initialize(std::filesystem::current_path(), opts);
      if (!result.success) {
        std::cerr << "[ERROR] " << result.error_message << "\n";
        return 1;
      }

      std::cout << "[INFO] Initialized project\n";
      std::cout << "  project: " << opts.project_name << "\n";
      std::cout << "  " << result.project_config_path << "\n";
      std::cout << "  " << result.tooling_config_path << "\n";
      std::cout << "  style: " << tooling::style_name(opts.style) << "\n";
      
      if (opts.enable_clang_tidy) {
        std::cout << " .clang-tidy (preset: " << tooling::preset_name(opts.tidy_preset) << ")\n";
      }
      std::cout << "  style: " << tooling::style_name(opts.style) << "\n";
      return 0;
    }

    // INFO: install command
    auto cfg = manager.load_project(config_path);
    auto repo_root = manager.find_git_root();

    if (subcmd == "install") {
      auto result = manager.install(repo_root, cfg);

      if (!result.error_message.empty()) {
        std::cerr << "[ERROR] " << result.error_message << "\n";
        return 1;
      }

      if (result.hook_installed) {
        std::cout << "[INFO] pre-commit hook installed at " << result.hook_path << "\n";
      }

      if (result.workflow_installed) {
        std::cout << "[INFO] workflow installed at " << result.workflow_path << "\n";
      }

      return 0;
    }

    // INFO: generate-gha command
    if (subcmd == "generate-gha") {
      auto wf_content = cicd::generate_github_actions(cfg, cicd::WorkflowConfig{});

      if (!cicd::write_workflow(repo_root, wf_content)) {
        std::cerr << "[ERROR] Failed to write GitHub Actions workflow\n";
        return 1;
      }

      std::cout << "[INFO] GitHub Actions workflow generated at "
                << (repo_root / ".github" / "workflows" / "sniffercommit.yml").string() << "\n";
      return 0;
    }

    // INFO: run command
    if (subcmd == "run") {
      bool all_files = false;
      bool verbose = false;
      bool dry_run = false;
      std::vector<std::string> run_files;

      for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (arg == "run") continue;
        if (arg == "--all-files")
          all_files = true;
        else if (arg == "--verbose" || arg == "-V")
          verbose = true;
        else if (arg == "--dry-run" || arg == "-n")
          dry_run = true;
        else if (!arg.starts_with('-'))
          run_files.emplace_back(arg);
      }

      RunOptions opts;
      opts.verbose = verbose;
      opts.dry_run = dry_run;
      opts.source = all_files ? FileSource::ALL_REPO
                              : (!run_files.empty() ? FileSource::EXPLICIT : FileSource::STAGED);

      if (!run_files.empty()) {
        opts.explicit_files = std::move(run_files);
      }

      auto files = collect_files(repo_root, opts, cfg.exclude_paths);

      if (verbose) {
        std::cout << fmt::format("[sniffercommit] Check {} file(s)\n", files.size());
      }

      return execute_checks(repo_root, cfg, files, opts);
    }

    app.show_help();
    return 1;

  } catch (const std::exception& error) {
    std::cerr << "[ERROR] " << error.what() << "\n";
    return 1;
  } catch (...) {
    std::cerr << "[ERROR] Unknown fatal error occurred\n";
    return 1;
  }
}
