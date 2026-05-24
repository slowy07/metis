#include <fmt/format.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <exception>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

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
constexpr std::string_view yellow = "\033[33m";
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

  std::cout << "\n  " << cyan << arrow << reset << " next: " << bold << "sniffercommit install"
            << reset << " " << dim << "to set up pre-commit hooks" << reset << "\n\n";
}

// parse c++ standard
sniffercommit::tooling::CppStandard parse_cpp_standard(const std::string& cpp_standard) {
  std::string lower;

  for (char chr : cpp_standard) {
    lower += static_cast<char>(std::tolower(static_cast<unsigned char>(chr)));
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

  for (char chr : type) {
    lower += static_cast<char>(std::tolower(static_cast<unsigned char>(chr)));
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

std::string prompt_string(std::string_view label, std::string_view default_val) {
  std::cout << "  " << bold << label << reset << " " << dim << "[" << default_val << "]" << reset
            << ": ";
  std::string input;
  std::getline(std::cin, input);

  if (input.empty()) {
    return std::string(default_val);
  }

  return input;
}

bool prompt_bool(std::string_view label, bool default_val) {
  std::cout << "  " << bold << label << reset << "  " << dim << "[" << (default_val ? "y" : "n")
            << "]" << reset << ": ";

  std::string input;
  std::getline(std::cin, input);

  if (input.empty()) {
    return default_val;
  }

  std::string lower;

  for (char chr : input) {
    lower += static_cast<char>(std::tolower(static_cast<unsigned char>(chr)));
  }

  return lower == "y" || lower == "yes" || lower == "true" || lower == "1";
}

template <typename T>
T prompt_int(std::string_view label, T default_val, T min_val, T max_val) {
  while (true) {
    std::cout << "  " << bold << label << reset << "  " << dim << "[" << default_val << "]" << reset
              << ": ";

    std::string input;
    std::getline(std::cin, input);

    if (input.empty()) {
      return default_val;
    }

    T val = 0;
    if (!safe_stoi(input.c_str(), val)) {
      std::cout << "    " << yellow << "!" << reset << " invalid number, try again\n";
      continue;
    }

    if (val < min_val || val > max_val) {
      std::cout << "    " << yellow << "!" << reset << " must be between " << min_val << " and "
                << max_val << ", try again\n";
      continue;
    }

    return val;
  }
}

std::string prompt_choice(std::string_view label, std::string_view default_val,
                          std::span<const std::string_view> choices) {
  std::cout << "  " << bold << label << reset << " " << dim << "[" << default_val << "]" << reset
            << " ";

  std::cout << "(";
  for (size_t i = 0; i < choices.size(); ++i) {
    if (i > 0) {
      std::cout << "|";
    }
    std::cout << choices[i];
  }
  std::cout << "): ";

  std::string input;
  std::getline(std::cin, input);

  if (input.empty()) {
    return std::string(default_val);
  }

  return input;
}

void prompt_depedencies(sniffercommit::ConfigManager::InitOptions& opts) {
  if (!prompt_bool("add depedencies", false)) {
    return;
  }

  std::cout << "\n  " << dim << "enter depdency info (empty name to finish)" << reset << "\n";

  while (true) {
    std::cout << "\n";
    std::string name = prompt_string("  dep name", "");

    if (name.empty()) {
      break;
    }

    std::string url =
        prompt_string("  git url", "https://github.com/" + name + "/" + name + ".git");
    std::string tag = prompt_string("  git tag", "main");

    sniffercommit::tooling::Depedency dep;
    dep.name = name;
    dep.git_url = url;
    dep.git_tag = tag;

    if (auto err = dep.validate(); !err.empty()) {
      std::cout << "    " << yellow << "!" << reset << " " << err << " — skipped\n";
      continue;
    }

    opts.depdencies.push_back(std::move(dep));
    std::cout << "    " << green << check << reset << " added " << name << "\n";
  }
}

void run_interactive_init(sniffercommit::ConfigManager::InitOptions& opts) {
  std::cout << "\n";
  std::cout << bold << "  sniffercommit init" << reset << "\n";
  std::cout << "  " << "enter to accept the default configs" << reset << "\n\n";

  // INFO: project
  opts.project_name = prompt_string("project name", opts.project_name);

  // INFO: style
  static constexpr std::string_view format_style[] = {"google", "llvm",      "chromium", "mozilla",
                                                      "webkit", "microsoft", "gnu"};

  std::string style_str = prompt_choice("formatter style", "google", format_style);
  std::ranges::transform(style_str, style_str.begin(),
                         [](unsigned char chr) { return std::tolower(chr); });

  try {
    opts.style = sniffercommit::tooling::parse_style(style_str);
  } catch (const std::exception&) {
    std::cout << "    " << yellow << "!" << reset << " Unknow style. using default\n";
    opts.style = sniffercommit::tooling::FormatterStyle::Google;
  }

  opts.indent_width = prompt_int("indent width", opts.indent_width, 1, 16);
  opts.column_limit = prompt_int("column limit", opts.column_limit, 20, 500);

  // INFO: clang tidy
  opts.enable_clang_tidy = prompt_bool("enable clang-tidy", opts.enable_clang_tidy);

  if (opts.enable_clang_tidy) {
    static constexpr std::string_view clang_tidy_preset[] = {"minimal", "standard", "strict",
                                                             "custom"};

    std::string preset_str = prompt_choice("tidy preset", "standard", clang_tidy_preset);

    try {
      opts.tidy_preset = parse_tidy_preset(preset_str);
    } catch (const std::exception&) {
      std::cout << "    " << yellow << "!" << reset << " Unknown preset. using default\n";
      opts.tidy_preset = sniffercommit::tooling::TidyPreset::Standard;
    }

    static constexpr std::string_view preset_severity[] = {"note", "warning", "error"};
    std::string sev_str = prompt_choice("tidy severity", "error", preset_severity);

    try {
      opts.tidy_severity = parse_tidy_severity(sev_str);
    } catch (const std::exception&) {
      std::cout << "    " << yellow << "!" << reset << " Unknown severity. using default\n";
      opts.tidy_severity = sniffercommit::tooling::TidySeverity::Error;
    }
  }

  // INFO: CMake
  opts.enable_cmake = prompt_bool("generate CMakeLists.txt", opts.enable_cmake);
  if (opts.enable_cmake) {
    opts.generate_source = true;

    static constexpr std::string_view preset_standard_cpp[] = {"17", "20", "23"};
    std::string std_str = prompt_choice("C++ Standard", "20", preset_standard_cpp);

    try {
      opts.cmake_cpp_standard = parse_cpp_standard(std_str);
    } catch (const std::exception&) {
      std::cout << "    " << yellow << "!" << reset << " Unknown standard, using C++20\n";
      opts.cmake_cpp_standard = sniffercommit::tooling::CppStandard::Cpp20;
    }

    static constexpr std::string_view preset_target[] = {"executable", "static", "shared",
                                                         "header-only"};
    std::string type_str = prompt_choice("target type", "executable", preset_target);

    try {
      opts.cmake_target_type = parse_target_type(type_str);
    } catch (const std::exception&) {
      std::cout << "    " << yellow << "!" << reset << " Unknown type, using executable\n";
      opts.cmake_target_type = sniffercommit::tooling::TargetType::Executable;
    }

    opts.cmake_enable_testing = prompt_bool("enable testing", opts.cmake_enable_testing);
    opts.cmake_enable_sanitizers = prompt_bool("enable sanitizers", opts.cmake_enable_sanitizers);

    prompt_depedencies(opts);
  }

  std::cout << "\n";
}

// NOTE: parse -add-dep name:ult:tag from CLI
bool parse_depedency_flag(const std::string& value, sniffercommit::ConfigManager::InitOptions& opts,
                          std::string& error_out) {
  auto first_colon = value.find(':');

  if (first_colon == std::string::npos) {
    error_out =
        "--add-dep format is name:url:tag (e.g. fmt:https://github.com/fmtlib/fmt.git:11.0.2)";
    return false;
  }

  auto second_colon = value.find(':', first_colon + 1);

  sniffercommit::tooling::Depedency dep;
  dep.name = value.substr(0, first_colon);

  if (second_colon == std::string::npos) {
    dep.git_url = value.substr(first_colon + 1);
    dep.git_tag = "main";
  } else {
    dep.git_url = value.substr(first_colon + 1, second_colon - first_colon - 1);
    dep.git_tag = value.substr(second_colon + 1);
  }

  if (auto err = dep.validate(); !err.empty()) {
    error_out = err;
    return false;
  }

  opts.depdencies.push_back(std::move(dep));
  return true;
}

// NOTE: CLI flag information parsing
bool parse_cli_flags(std::span<char*> args, size_t argc_sz,
                     sniffercommit::ConfigManager::InitOptions& opts) {
  for (size_t i = 1; i < argc_sz; ++i) {
    std::string arg = args[i];

    if (arg == "--style") {
      if (i + 1 >= argc_sz) {
        std::cerr << "[ERROR] --style requires value\n";
        return false;
      }
      ++i;
      std::string value = args[i];
      std::ranges::transform(value, value.begin(),
                             [](unsigned char chr) { return std::tolower(chr); });
      try {
        opts.style = sniffercommit::tooling::parse_style(value);
      } catch (const std::exception& error_style_config) {
        std::cerr << "[ERROR] " << error_style_config.what() << "\n";
        return false;
      }
    }

    if (arg == "--indent-width") {
      if (i + 1 >= argc_sz) {
        std::cerr << "[ERROR] --indent-width require integer value\n";
        return false;
      }
      ++i;
      if (!safe_stoi(args[i], opts.indent_width)) {
        std::cerr << "[ERROR] --indent-width require integer value\n";
        return false;
      }
    }

    if (arg == "--column-limit") {
      if (i + 1 >= argc_sz) {
        std::cerr << "[ERROR] --column-limit requires integer value\n";
        return false;
      }
      ++i;
      if (!safe_stoi(args[i], opts.column_limit)) {
        std::cerr << "[ERROR] --column-limit requires integer value\n";
        return false;
      }
    }

    if (arg == "--pointer-alignment") {
      if (i + 1 >= argc_sz) {
        std::cerr << "[ERROR] --pointer-alignment requires value\n";
        return false;
      }
      ++i;
      opts.pointer_alignment = args[i];
    }

    if (arg == "--brace-style") {
      if (i + 1 >= argc_sz) {
        std::cerr << "[ERROR] --brace-style requires value\n";
        return false;
      }
      ++i;
      opts.brace_style = args[i];
    }

    if (arg == "--name") {
      if (i + 1 >= argc_sz) {
        std::cerr << "[ERROR] --name-requires value\n";
        return false;
      }
      ++i;
      opts.project_name = args[i];
    }

    if (arg == "--enable-clang-tidy" || arg == "--tidy") {
      opts.enable_clang_tidy = true;
    }

    if (arg == "--tidy-preset") {
      if (i + 1 >= argc_sz) {
        std::cerr << "[ERROR] --tidy-preset requires value (minimal|standard|strict|custom)\n";
        return false;
      }
      try {
        ++i;
        opts.tidy_preset = parse_tidy_preset(args[i]);
      } catch (const std::exception& error_tidy_preset) {
        std::cerr << "[ERROR] " << error_tidy_preset.what() << "\n";
        return false;
      }
    }

    if (arg == "--tidy-severity") {
      if (i + 1 >= argc_sz) {
        std::cerr << "[ERROR] --tidy-severity requires value (note|warning|error)\n";
        return false;
      }
      try {
        ++i;
        opts.tidy_severity = parse_tidy_severity(args[i]);
      } catch (const std::exception& error_tidy_severity) {
        std::cerr << "[ERROR] " << error_tidy_severity.what() << "\n";
        return false;
      }
    }

    if (arg == "--tidy-header-filter") {
      if (i + 1 >= argc_sz) {
        std::cerr << "[ERROR] --tidy-header-filter requires integer (0|1|2)\n";
        return false;
      }
      ++i;
      if (!safe_stoi(args[i], opts.tidy_header_filter)) {
        std::cerr << "[ERROR] --tidy-header-filter requires integer (0|1|2)\n";
        return false;
      }
      if (opts.tidy_header_filter < 0 || opts.tidy_header_filter > 2) {
        std::cerr << "[ERROR] --tidy-header-filter must be 0, 1, or 2\n";
        return false;
      }
    }

    if (arg == "--enable-cmake" || arg == "--cmake") {
      opts.enable_cmake = true;
      opts.generate_source = true;
    }

    if (arg == "--cmake-cpp-standard") {
      if (i + 1 >= argc_sz) {
        std::cerr << "[ERROR] --cmake-cpp-standard requires value (17|20|23)\n";
        return false;
      }
      try {
        ++i;
        opts.cmake_cpp_standard = parse_cpp_standard(args[i]);
      } catch (const std::exception& error_parse_cpp_standard) {
        std::cerr << "[ERROR] " << error_parse_cpp_standard.what() << "\n";
        return false;
      }
    }

    if (arg == "--cmake-target-type") {
      if (i + 1 >= argc_sz) {
        std::cerr << "[ERROR] --cmake-target-type requires value "
                     "(executable|static|shared|header-only)\n";
        return false;
      }
      try {
        ++i;
        opts.cmake_target_type = parse_target_type(args[i]);
      } catch (const std::exception& error_parse_target_type) {
        std::cerr << "[ERROR] " << error_parse_target_type.what() << "\n";
        return false;
      }
    }

    if (arg == "--cmake-enable-testing") {
      opts.cmake_enable_testing = true;
    }

    if (arg == "--cmake-enable-sanitizers") {
      opts.cmake_enable_sanitizers = true;
    }

    if (arg == "--add-dep") {
      if (i + 1 >= argc_sz) {
        std::cerr << "[ERROR] --add-dep requires value (name:url:tag)\n";
        return false;
      }

      ++i;
      std::string dep_error;
      if (!parse_depedency_flag(args[i], opts, dep_error)) {
        std::cerr << "[ERROR] " << dep_error << "\n";
        return false;
      }
    }

    if (arg == "--interactive" || arg == "-i") {
      // TODO: handled before this function
    }
  }
  return true;
}

}  // namespace

int main(int argc, char** argv) {  // NOLINT(readability-function-cognitive-complexity)
  using namespace sniffercommit;

  auto argc_sz = static_cast<size_t>(argc);
  std::span args(argv, argc_sz);
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

      // Check for --interactive or if no extra flags beyond "init"
      bool interactive = false;
      for (size_t i = 1; i < argc_sz; ++i) {
        std::string_view arg = args[i];
        if (arg == "--interactive" || arg == "-i") {
          interactive = true;
          break;
        }
      }

      // If only "init" with no flags, default to interactive mode
      bool has_flags = false;
      for (size_t i = 1; i < argc_sz; ++i) {
        std::string_view arg = args[i];
        if (arg.starts_with("--") || arg.starts_with('-')) {
          has_flags = true;
          break;
        }
      }

      if (interactive || !has_flags) {
        run_interactive_init(opts);
      } else {
        if (!parse_cli_flags(args, argc_sz, opts)) {
          return 1;
        }
      }

      auto result = ConfigManager::initialize(std::filesystem::current_path(), opts);
      if (!result.success) {
        std::cerr << "[ERROR] " << result.error_message << "\n";
        return 1;
      }

      print_init_summary(opts, result);
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
      bool format_mode = false;
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
        } else if (arg == "--format" || arg == "-f") {
          format_mode = true;
        } else if (!arg.starts_with('-')) {
          run_files.emplace_back(arg);
        }
      }

      RunOptions opts;
      opts.verbose = verbose;
      opts.dry_run = dry_run;
      opts.mode = format_mode ? RunMode::FORMAT : RunMode::CHECK;

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

      if (opts.mode == RunMode::FORMAT) {
        return execute_format(repo_root, files, opts);
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
