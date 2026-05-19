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

std::string preparse_config_path(std::span<char*> args) {
  std::string config_path = ".sniffercommit.toml";

  // NOLINTBEGIN(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
  for (size_t i = 1; i + 1 < args.size(); ++i) {
    std::string_view arg = args[i];
    if ((arg == "-c" || arg == "--config") && i + 1 < args.size()) {
      config_path = args[i + 1];
      break;
    }
  }
  // NOLINTEND(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
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

  for (char chr : preset) {
    lower += static_cast<char>(std::tolower(static_cast<unsigned char>(chr)));
  }

  if (lower == "minimal") {
    return sniffercommit::tooling::TidyPreset::Minimal;
  }
  if (lower == "standard") {
    return sniffercommit::tooling::TidyPreset::Standard;
  }
  if (lower == "strict") {
    return sniffercommit::tooling::TidyPreset::Strict;
  }
  if (lower == "custom") {
    return sniffercommit::tooling::TidyPreset::Custom;
  }

  throw std::runtime_error("Unknown tidy preset: " + preset);
}

sniffercommit::tooling::TidySeverity parse_tidy_severity(const std::string& sev) {
  std::string lower;

  for (char chr : sev) {
    lower += static_cast<char>(std::tolower(static_cast<unsigned char>(chr)));
  }

  if (lower == "note") {
    return sniffercommit::tooling::TidySeverity::Note;
  }
  if (lower == "warning") {
    return sniffercommit::tooling::TidySeverity::Warning;
  }
  if (lower == "error") {
    return sniffercommit::tooling::TidySeverity::Error;
  }

  throw std::runtime_error("Unknown tidy severity: " + sev);
}

constexpr std::string_view bold = "\033[1m";
constexpr std::string_view dim = "\033[2m";
constexpr std::string_view green = "\033[32m";
constexpr std::string_view cyan = "\033[36m";
constexpr std::string_view reset = "\033[0m";
constexpr std::string_view check = "✓";
constexpr std::string_view arrow = "→";
constexpr std::string_view bullet = "•";

void print_init_summary(const sniffercommit::ConfigManager::InitOptions& opts,
                        const sniffercommit::ConfigManager::InitResult& result) {
  std::cout << "\n";
  std::cout << bold << " sniffercommit initialized " << reset << "\n";
  std::cout << "  " << dim << result.project_config_path << reset << "\n\n";

  std::cout << "  " << bold << "project" << reset << "\n";
  std::cout << "    " << bullet << " name:   " << opts.project_name << "\n";
  std::cout << "    " << bullet << " style:  " << sniffercommit::tooling::style_name(opts.style)
            << "\n";

  std::cout << "\n  " << bold << "tooling" << reset << "\n";
  std::cout << "    " << green << check << reset << " .clang-format";

  if (opts.indent_width != 2 || opts.column_limit != 100) {
    std::cout << "  (indent=" << opts.indent_width << ", limit=" << opts.column_limit << ")";
  }
  std::cout << "\n";

  if (opts.enable_clang_tidy) {
    std::cout << "    " << green << check << reset << " .clang-tidy"
              << "  (preset: " << sniffercommit::tooling::preset_name(opts.tidy_preset)
              << ", severity: ";

    switch (opts.tidy_severity) {
      case sniffercommit::tooling::TidySeverity::Note:
        std::cout << "note";
        break;
      case sniffercommit::tooling::TidySeverity::Warning:
        std::cout << "warning";
        break;
      case sniffercommit::tooling::TidySeverity::Error:
        std::cout << "error";
        break;
    }

    std::cout << ")\n";
  }

  if (opts.generate_source && !result.src_path.empty()) {
    std::cout << "\n  " << bold << "source" << reset << "\n";
    std::cout << "    " << green << check << reset << " " << result.src_path << "\n";
    std::cout << "    " << dim << "      " << arrow << " ready to build & run" << reset << "\n";
  }

  std::cout << "\n  " << cyan << arrow << reset << " next: " << bold << "sniffercommit install"
            << reset << " " << dim << "to set up pre-commit hooks" << reset << "\n\n";
}

// parse c++ standard
sniffercommit::tooling::CppStandard parse_cpp_standard(const std::string& cpp_standard) {
  std::string lower;

  for (char c : cpp_standard) {
    lower += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  if (lower == "17" || lower == "c++17") {
    return sniffercommit::tooling::CppStandard::Cpp17;
  }
  if (lower == "20" || lower == "c++20") {
    return sniffercommit::tooling::CppStandard::Cpp20;
  }
  if (lower == "23" || lower == "c++23") {
    return sniffercommit::tooling::CppStandard::Cpp23;
  }
  throw std::runtime_error("Unknown C++ standard: " + cpp_standard);
}

// parse target type
sniffercommit::tooling::TargetType parse_target_type(const std::string& type) {
  std::string lower;

  for (char c : type) {
    lower += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }

  if (lower == "executable" || lower == "exe") {
    return sniffercommit::tooling::TargetType::Executable;
  }

  if (lower == "static" || lower == "staticlib") {
    return sniffercommit::tooling::TargetType::StaticLibrary;
  }

  if (lower == "shared" || lower == "sharedlib") {
    return sniffercommit::tooling::TargetType::SharedLibrary;
  }

  if (lower == "header-only" || lower == "interface") {
    return sniffercommit::tooling::TargetType::HeaderOnly;
  }

  throw std::runtime_error("Unknown target type: " + type);
}

}  // namespace

int main(int argc, char** argv) {  // NOLINT(readability-function-cognitive-complexity)
  using namespace sniffercommit;

  auto argc_sz = static_cast<size_t>(argc);
  std::span<char*> args(argv, argc_sz);
  std::string config_path = preparse_config_path(args);

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

  // NOLINTBEGIN(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
  try {
    auto subcmd = app.get_subcommand();
    // INFO: init command
    if (subcmd == "init") {
      ConfigManager::InitOptions opts;
      opts.project_name = std::filesystem::current_path().filename().string();

      for (size_t i = 1; i < argc_sz; ++i) {
        std::string arg = args[i];

        // NOTE: stye command `--style`
        if (arg == "--style") {
          if (i + 1 >= argc_sz) {
            std::cerr << "[ERROR] --style requires value\n";
            return 1;
          }

          ++i;
          std::string value = args[i];
          std::ranges::transform(value, value.begin(),
                                 [](unsigned char chr) { return std::tolower(chr); });

          try {
            opts.style = tooling::parse_style(value);
          } catch (const std::exception& error_style_config) {
            std::cerr << "[ERROR] " << error_style_config.what() << "\n";
            return 1;
          }
        }

        // NOTE: indent width command `--indent-width`
        if (arg == "--indent-width") {
          if (i + 1 >= argc_sz) {
            std::cerr << "[ERROR] --indent-width require integer value\n";
            return 1;
          }
          ++i;
          if (!safe_stoi(args[i], opts.indent_width)) {
            std::cerr << "[ERROR] --indent-width require integer value\n";
            return 1;
          }
        }

        // NOTE: column limit command `--column-limit`
        if (arg == "--column-limit") {
          if (i + 1 >= argc_sz) {
            std::cerr << "[ERROR] --column-limit requires integer value\n";
            return 1;
          }
          ++i;
          if (!safe_stoi(args[i], opts.column_limit)) {
            std::cerr << "[ERROR] --column-limit requires integer value\n";
            return 1;
          }
        }

        // NOTE: pointer alignment command `--pointer-alignment`
        if (arg == "--pointer-alignment") {
          if (i + 1 >= argc_sz) {
            std::cerr << "[ERROR] --pointer-alignment requires value\n";
            return 1;
          }
          ++i;
          opts.pointer_alignment = args[i];
        }

        // NOTE: brace style command `--brace-style`
        if (arg == "--brace-style") {
          if (i + 1 >= argc_sz) {
            std::cerr << "[ERROR] --brace-style requires value\n";
            return 1;
          }
          ++i;
          opts.brace_style = args[i];
        }

        // NOTE: project name command `--name`
        if (arg == "--name") {
          if (i + 1 >= argc_sz) {
            std::cerr << "[ERROR] --name-requires value\n";
            return 1;
          }

          ++i;
          opts.project_name = args[i];
        }

        // NOTE: clang tidy command "--enable-clang-tidy"
        if (arg == "--enable-clang-tidy" || arg == "--tidy") {
          opts.enable_clang_tidy = true;
        }

        if (arg == "--tidy-preset") {
          if (i + 1 >= argc_sz) {
            std::cerr << "[ERROR] --tidy-preset requires value (minimal|standard|strict|custom)\n";
            return 1;
          }

          try {
            ++i;
            opts.tidy_preset = parse_tidy_preset(args[i]);
          } catch (const std::exception& error_tidy_preset) {
            std::cerr << "[ERROR] " << error_tidy_preset.what() << "\n";
            return 1;
          }
        }

        if (arg == "--tidy-severity") {
          if (i + 1 >= argc_sz) {
            std::cerr << "[ERROR] --tidy-severity requires value (note|warning|error)\n";
            return 1;
          }

          try {
            ++i;
            opts.tidy_severity = parse_tidy_severity(args[i]);
          } catch (const std::exception& error_tidy_severity) {
            std::cerr << "[ERROR] " << error_tidy_severity.what() << "\n";
            return 1;
          }
        }

        if (arg == "--tidy-header-filter") {
          if (i + 1 >= argc_sz) {
            std::cerr << "[ERROR] --tidy-header-filter requires integer (0|1|2)\n";
            return 1;
          }
          ++i;
          if (!safe_stoi(args[i], opts.tidy_header_filter)) {
            std::cerr << "[ERROR] --tidy-header-filter requires integer (0|1|2)\n";
            return 1;
          }

          if (opts.tidy_header_filter < 0 || opts.tidy_header_filter > 2) {
            std::cerr << "[ERROR] --tidy-header-filter must be 0, 1, or 2\n";
            return 1;
          }
        }

        if (arg == "--enable-cmake" || arg == "--cmake") {
          opts.enable_cmake = true;
          opts.generate_source = true;
        }

        if (arg == "--cmake-cpp-standard") {
          if (i + 1 >= argc_sz) {
            std::cerr << "[ERROR] --cmake-cpp-standard requires value (17|20|23)\n";
            return 1;
          }

          try {
            ++i;
            opts.cmake_cpp_standard = parse_cpp_standard(args[i]);
          } catch (const std::exception& error_parse_cpp_standard) {
            std::cerr << "[ERROR] " << error_parse_cpp_standard.what() << "\n";
            return 1;
          }
        }

        if (arg == "--cmake-target-type") {
          if (i + 1 >= argc_sz) {
            std::cerr << "[ERROR] --cmake-target-type requires value "
                         "(executable|static|shared|header-only)\n";
            return 1;
          }

          try {
            ++i;
            opts.cmake_target_type = parse_target_type(args[i]);
          } catch (const std::exception& error_parse_target_type) {
            std::cerr << "[ERROR] " << error_parse_target_type.what() << "\n";
            return 1;
          }
        }

        if (arg == "--cmake-enable-testing") {
          opts.cmake_enable_testing = true;
        }

        if (arg == "--cmake-enable-sanitizers") {
          opts.cmake_enable_sanitizers = true;
        }

        if (arg == "--generate-src") {
          opts.generate_source = true;
        }
      }

      auto result = ConfigManager::initialize(std::filesystem::current_path(), opts);
      if (!result.success) {
        std::cerr << "[ERROR] " << result.error_message << "\n";
        return 1;
      }

      print_init_summary(opts, result);

      if (opts.enable_clang_tidy) {
        std::cout << " .clang-tidy (preset: " << tooling::preset_name(opts.tidy_preset) << ")\n";
      }

      if (opts.generate_source && !result.src_path.empty()) {
        std::cout << " " << result.src_path << "\n";
      }

      if (opts.enable_cmake) {
        std::cout << " " << result.cmake_config_path << "\n";
      }

      std::cout << "  style: " << tooling::style_name(opts.style) << "\n";
      return 0;
    }

    // INFO: install command
    auto cfg = ConfigManager::load_project(config_path);
    auto repo_root = ConfigManager::find_git_root();

    if (subcmd == "install") {
      auto result = ConfigManager::install(repo_root, cfg);

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

      for (size_t i = 1; i < argc_sz; ++i) {
        std::string_view arg = args[i];
        if (arg == "run") {
          continue;
        }
        if (arg == "--all-files") {
          all_files = true;
        } else if (arg == "--verbose" || arg == "-V") {
          verbose = true;
        } else if (arg == "--dry-run" || arg == "-n") {
          dry_run = true;
        } else if (!arg.starts_with('-')) {
          run_files.emplace_back(arg);
        }
      }

      RunOptions opts;
      opts.verbose = verbose;
      opts.dry_run = dry_run;
      if (all_files) {
        opts.source = FileSource::ALL_REPO;
      } else if (!run_files.empty()) {
        opts.source = FileSource::EXPLICIT;
      } else {
        opts.source = FileSource::STAGED;
      }

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
  // NOLINTEND(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
}
