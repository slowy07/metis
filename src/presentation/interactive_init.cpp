#include "metis/presentation/interactive_init.hpp"

#include <fmt/format.h>

#include <array>
#include <cctype>
#include <cstring>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include "metis/application/init_use_case.hpp"
#include "metis/presentation/console.hpp"
#include "metis/presentation/summary_reporter.hpp"

namespace metis::presentation {

namespace {

using C = Console;

// Safe string-to-int conversion with error handling.
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

std::string prompt_string(std::string_view label, std::string_view default_val) {
  std::cout << "  " << C::bold(std::string(label)) << " "
            << C::dim("[" + std::string(default_val) + "]") << ": ";
  std::string input;
  std::getline(std::cin, input);
  if (input.empty()) {
    return std::string(default_val);
  }
  return input;
}

bool prompt_bool(std::string_view label, bool default_val) {
  std::cout << "  " << C::bold(std::string(label)) << "  "
            << C::dim("[" + std::string(default_val ? "y" : "n") + "]") << ": ";
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
T prompt_int(std::string_view label, T default_val, std::pair<T, T> range) {
  auto [min_val, max_val] = range;
  while (true) {
    std::cout << "  " << C::bold(std::string(label)) << "  "
              << C::dim("[" + std::to_string(default_val) + "]") << ": ";
    std::string input;
    std::getline(std::cin, input);
    if (input.empty()) {
      return default_val;
    }
    T val = 0;
    if (!safe_stoi(input.c_str(), val)) {
      std::cout << "    " << C::yellow("!") << " " << C::dim("Invalid number, try again") << "\n";
      continue;
    }
    if (val < min_val || val > max_val) {
      std::cout << "    " << C::yellow("!") << " "
                << C::dim("Must be between " + std::to_string(min_val) + " and " +
                          std::to_string(max_val) + ", try again")
                << "\n";
      continue;
    }
    return val;
  }
}

std::string prompt_choice(std::string_view label, std::string_view default_val,
                          std::span<const std::string_view> choices) {
  std::cout << "  " << C::bold(std::string(label)) << " "
            << C::dim("[" + std::string(default_val) + "]") << " (";
  bool first = true;
  for (auto choice : choices) {
    if (!first) {
      std::cout << "|";
    }
    first = false;
    std::cout << choice;
  }
  std::cout << "): ";
  std::string input;
  std::getline(std::cin, input);
  return input.empty() ? std::string(default_val) : input;
}

void prompt_dependencies(application::InitOptions& opts) {
  if (!prompt_bool("Add dependencies", false)) {
    return;
  }
  std::cout << "\n  " << C::dim("Enter dependency info (empty name to finish)") << "\n";
  while (true) {
    std::cout << "\n";
    std::string name = prompt_string("  Dep name", "");
    if (name.empty()) {
      break;
    }

    std::string default_url = fmt::format("https://github.com/{0}/{0}.git", name);
    std::string url = prompt_string("  Git URL", default_url);
    std::string tag = prompt_string("  Git tag", "main");
    std::string dep = fmt::format("{}:{}:{}", name, url, tag);
    opts.dependencies.push_back(std::move(dep));
    std::cout << "    " << C::green("✓") << " Added " << C::bold(name) << "\n";
  }
}

}  // namespace

void run_interactive_init(application::InitOptions& opts) {
#ifdef _WIN32
  enable_windows_ansi();
#endif

  C::print_header("metis init wizard");
  std::cout << C::dim("Press Enter to accept defaults shown in [brackets]") << "\n\n";

  opts.project_name = prompt_string("Project name", opts.project_name);

  static constexpr std::array format_style = {
      std::string_view{"google"},  std::string_view{"llvm"},   std::string_view{"chromium"},
      std::string_view{"mozilla"}, std::string_view{"webkit"}, std::string_view{"microsoft"},
      std::string_view{"gnu"}};

  opts.style = prompt_choice("Formatter style", "google", format_style);
  opts.indent_width = prompt_int("Indent width", opts.indent_width, std::pair{1, 16});
  opts.column_limit = prompt_int("Column limit", opts.column_limit, std::pair{20, 500});

  opts.enable_clang_tidy = prompt_bool("Enable clang-tidy", opts.enable_clang_tidy);

  if (opts.enable_clang_tidy) {
    static constexpr auto tidy_presets =
        std::to_array<std::string_view>({"minimal", "standard", "strict", "custom"});
    std::string preset = prompt_choice("Tidy preset", "standard", tidy_presets);
    if (preset == "minimal" || preset == "standard" || preset == "strict" || preset == "custom") {
      opts.tidy_preset = preset;
    }

    static constexpr auto severity_presets =
        std::to_array<std::string_view>({"note", "warning", "error"});
    opts.tidy_severity = prompt_choice("Tidy severity", "error", severity_presets);
  }

  opts.enable_cmake = prompt_bool("Generate CMakeLists.txt", opts.enable_cmake);
  if (opts.enable_cmake) {
    opts.generate_source = true;
    static constexpr auto standards = std::to_array<std::string_view>({"17", "20", "23"});
    opts.cmake_cpp_standard = prompt_choice("C++ standard", "20", standards);

    static constexpr auto targets =
        std::to_array<std::string_view>({"executable", "static", "shared", "header-only"});
    opts.cmake_target_type = prompt_choice("Target type", "executable", targets);

    opts.cmake_enable_testing = prompt_bool("Enable testing", opts.cmake_enable_testing);
    opts.cmake_enable_sanitizers = prompt_bool("Enable sanitizers", opts.cmake_enable_sanitizers);
    prompt_dependencies(opts);
  }

  opts.enable_conan = prompt_bool("Generate conanfile.py", opts.enable_conan);

  opts.enable_compiler_checks = prompt_bool("Enable compiler checks", opts.enable_compiler_checks);
  if (opts.enable_compiler_checks) {
    static constexpr auto compilers =
        std::to_array<std::string_view>({"g++", "clang++", "gcc", "clang"});
    opts.compiler = prompt_choice("Compiler", "g++", compilers);

    static constexpr auto standards = std::to_array<std::string_view>({"17", "20", "23", "26"});
    opts.compiler_cpp_standard = prompt_choice("C++ standard", "20", standards);

    opts.compiler_werror = prompt_bool("Treat warnings as errors (-Werror)", opts.compiler_werror);
    opts.compiler_debug_and_release =
        prompt_bool("Check both debug and release configurations", opts.compiler_debug_and_release);
  }

  std::cout << "\n";
}

void print_init_summary(const application::InitOptions& opts,
                        const application::InitResult& result) {
  SummaryReporter::InitSummary summary;
  summary.project_name = opts.project_name.empty() ? "unnamed" : opts.project_name;
  summary.style = opts.style;
  summary.clang_tidy = opts.enable_clang_tidy;
  summary.tidy_preset = opts.tidy_preset;
  summary.cmake = opts.enable_cmake;
  summary.conan = opts.enable_conan;
  summary.compiler_checks = opts.enable_compiler_checks;

  if (!result.project_config_path.empty()) {
    summary.generated_files.emplace_back(".metis.toml");
  }
  if (!result.tooling_config_path.empty()) {
    summary.generated_files.emplace_back(".clang-format");
  }
  if (!result.cmake_config_path.empty()) {
    summary.generated_files.emplace_back("CMakeLists.txt");
  }
  if (!result.conan_config_path.empty()) {
    summary.generated_files.emplace_back("conanfile.py");
  }
  if (!result.src_path.empty()) {
    summary.generated_files.emplace_back("src/main.cpp");
  }

  SummaryReporter::print_init_summary(summary);
}

}  // namespace metis::presentation
