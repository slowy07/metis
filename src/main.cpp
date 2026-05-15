#include <fmt/format.h>

#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "sniffercommit/argparser.hpp"
#include "sniffercommit/config.hpp"
#include "sniffercommit/executor.hpp"
#include "sniffercommit/generator.hpp"
#include "sniffercommit/installer.hpp"
#include "sniffercommit/template.hpp"

int main(int argc, char** argv) {
  using namespace sniffercommit;

  std::string config_path = ".sniffercommit.toml";

  ArgParser app("sniffercommit", "Fast C++20-powered pre-commit & CI generator");
  app.set_version("0.1.0")
      .add_option("-c", "--config", "Config file path", config_path)
      .add_subcommand("init", "Create default .sniffercommit.toml")
      .add_subcommand("install", "Generate & install .git/hooks/pre-commit")
      .add_subcommand("generate-gha", "Output GitHub Actions workflow")
      .add_subcommand("run", "Execute checks on files");

  if (!app.parse(argc, argv)) return 0;

  try {
    auto subcmd = app.get_subcommand();

    // INIT
    if (subcmd == "init") {
      std::string project_name = std::filesystem::current_path().filename().string();
      constexpr auto config_path = ".sniffercommit.toml";
      constexpr auto clang_format_path = ".clang-format";
      std::string formatter_style = "google";

      for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        // INFO: style formatter
        if (arg == "--style") {
          if (i + 1 >= argc) {
            std::cerr << "[ERROR] --style require value\n";
            return 1;
          }

          formatter_style = argv[++i];
        }

        // INFO: project name
        if (arg == "--name") {
          if (i + 1 >= argc) {
            std::cerr << "[ERROR] --name require value\n";
            return 1;
          }

          project_name = argv[++i];
        }
      }

      // INFO: generate toml files
      {
        std::ofstream config_file(config_path);

        if (!config_file) {
          std::cerr << "[ERROR] failed to create " << config_path << "\n";
          return 1;
        }

        config_file << sniffercommit::default_sniffercommit_config(project_name);
      }

      // INFO: generate clang-format file
      {
        std::ofstream clang_format_file(clang_format_path);

        if (!clang_format_file) {
          std::cerr << "[ERROR] failed to create " << clang_format_path << "\n";
          return 1;
        }

        try {
          clang_format_file << sniffercommit::default_clang_format(
              sniffercommit::parse_formatter_style(formatter_style));
        } catch (const std::exception& error_format_file) {
          std::cerr << "[ERROR] " << error_format_file.what() << "\n";
          return 1;
        }
      }

      std::cout << "[INFO] created " << config_path << "\n";
      std::cout << "  project: " << project_name << "\n";
      std::cout << "  - .sniffercommit.toml\n";
      std::cout << "  - .clang-format\n";
      std::cout << "     style: " << formatter_style << "\n";
      return 0;
    }

    auto cfg = load_config(config_path);
    auto repo_root = find_git_root();

    // INSTALL (Strict Routing)
    if (subcmd == "install") {
      if (cfg.generate_local_hook) {
        auto script = generate_local_hook(cfg);
        if (install_local_hook(repo_root, script)) {
          std::cout << "[INFO] pre-commit hook installed at .git/hooks/pre-commit\n";
        } else {
          std::cerr << "[ERROR] failed to write hook\n";
          return 1;
        }
      }
      if (cfg.generate_gha) {
        auto yml = generate_github_actions(cfg);
        if (write_github_actions(repo_root, yml)) {
          std::cout << "[INFO] github action workflow generated at "
                       ".github/workflows/sniffercommit.yml\n";
        } else {
          std::cerr << "[ERROR] failed to write github action workflow\n";
          return 1;
        }
      }
      return 0;
    }

    // GENERATE-GHA (Strict Routing)
    if (subcmd == "generate-gha") {
      auto yml = generate_github_actions(cfg);
      if (write_github_actions(repo_root, yml)) {
        std::cout << "[INFO] github action workflow generated at "
                     ".github/workflows/sniffercommit.yml\n";
      } else {
        std::cerr << "[ERROR] failed to write github action workflow\n";
        return 1;
      }
      return 0;
    }

    // RUN (Manual Subcommand Parsing)
    if (subcmd == "run") {
      bool all_files = false;
      bool verbose = false;
      bool dry_run = false;
      std::vector<std::string> run_files;

      // Manually parse remaining args for 'run'
      for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (arg == "run") continue;
        if (arg == "--all-files")
          all_files = true;
        else if (arg == "--verbose" || arg == "-V")
          verbose = true;
        else if (arg == "--dry-run" || arg == "-n")
          dry_run = true;
        else if (!arg.starts_with('-')) {
          run_files.emplace_back(arg);
        }
      }

      RunOptions opts;
      opts.verbose = verbose;
      opts.dry_run = dry_run;
      opts.source = all_files ? FileSource::ALL_REPO
                              : (!run_files.empty() ? FileSource::EXPLICIT : FileSource::STAGED);
      if (!run_files.empty()) opts.explicit_files = std::move(run_files);

      auto files = collect_files(repo_root, opts, cfg.exclude_paths);

      if (verbose) {
        std::cout << fmt::format("[sniffercommit] Checking {} file(s)\n", files.size());
      }

      int result = execute_checks(repo_root, cfg, files, opts);
      return result;
    }

    // Fallback
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
