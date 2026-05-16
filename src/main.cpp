#include <fmt/format.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>

#include "sniffercommit/argparser.hpp"
#include "sniffercommit/config.hpp"
#include "sniffercommit/executor.hpp"
#include "sniffercommit/generator.hpp"
#include "sniffercommit/installer.hpp"
#include "sniffercommit/template.hpp"

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

}  // namespace

int main(int argc, char** argv) {
  using namespace sniffercommit;

  std::string config_path = preparse_config_path(argc, argv);

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
      constexpr auto default_config_path = ".sniffercommit.toml";
      constexpr auto default_clang_format_path = ".clang-format";
      std::string formatter_style = "google";
      sniffercommit::ClangFormatConfig clang_format_cfg;

      for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        // INFO: style formatter
        if (arg == "--style") {
          if (i + 1 >= argc) {
            std::cerr << "[ERROR] --style require value\n";
            return 1;
          }

          std::string value = argv[++i];
          std::ranges::transform(value, value.begin(),
                                 [](unsigned char c) { return std::tolower(c); });

          try {
            clang_format_cfg.style = sniffercommit::parse_formatter_style(value);
          } catch (const std::exception& error_init_style) {
            std::cerr << "[ERROR] " << error_init_style.what() << "\n";
            return 1;
          }
        }

        // INFO: indent width
        if (arg == "--indent-width") {
          if (i + 1 >= argc) {
            std::cerr << "[ERROR] --indent-width requires value\n";
            return 1;
          }

          if (!safe_stoi(argv[++i], clang_format_cfg.ident_width)) {
            std::cerr << "[ERROR] --indent-width requires integer value, got: " << argv[i] << "\n";
            return 1;
          }
        }

        // INFO: column limit
        if (arg == "--column-limit") {
          if (i + 1 >= argc) {
            std::cerr << "[ERROR] --column-limit requires value\n";
            return 1;
          }

          if (!safe_stoi(argv[++i], clang_format_cfg.column_limit)) {
            std::cerr << "[ERROR] --column_limit requires integer value, got: " << argv[i] << "\n";
            return 1;
          }
        }

        // INFO: pointer alignment
        if (arg == "--pointer-alignment") {
          if (i + 1 >= argc) {
            std::cerr << "[ERROR] --pointer-alignment requires value\n";
            return 1;
          }

          clang_format_cfg.pointer_alignment = argv[++i];
        }

        // INFO: brace style
        if (arg == "--brace-style") {
          if (i + 1 >= argc) {
            std::cerr << "[ERROR] --brace-style requires value\n";
            return 1;
          }

          clang_format_cfg.break_before_braces = argv[++i];
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

        config_file << sniffercommit::default_sniffercommit_config(project_name, clang_format_cfg);
      }

      // INFO: generate clang-format file
      {
        std::ofstream clang_format_file(default_clang_format_path);
        if (!clang_format_file) {
          std::cerr << "[ERROR] failed to create " << default_clang_format_path << "\n";
          return 1;
        }

        try {
          clang_format_file << generate_clang_format(clang_format_cfg);
        } catch (const std::exception& error_format_file) {
          std::cerr << "[ERROR] " << error_format_file.what() << "\n";
          return 1;
        }
      }

      std::cout << "[INFO] created " << config_path << "\n";
      std::cout << "  project: " << project_name << "\n";
      std::cout << "  - .sniffercommit.toml\n";
      std::cout << "  - .clang-format\n";
      std::cout << "     style: " << sniffercommit::formatter_style_name(clang_format_cfg.style)
                << "\n";
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
