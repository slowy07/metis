#include "metis/presentation/interactive_init.hpp"

#include <array>
#include <cctype>
#include <cstring>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include "metis/application/init_use_case.hpp"

namespace metis::presentation {

namespace {

// ANSI color codes for terminal output
constexpr std::string_view bold = "\033[1m";
constexpr std::string_view dim = "\033[2m";
constexpr std::string_view green = "\033[32m";
constexpr std::string_view yellow = "\033[33m";
constexpr std::string_view cyan = "\033[36m";
constexpr std::string_view reset = "\033[0m";
constexpr std::string_view check = "✓";
constexpr std::string_view arrow = "→";
constexpr std::string_view bullet = "•";

// Safe string-to-int conversion with error handling.
// Returns false if the string isn't a valid integer.
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

// Prompts the user for a string value with a default.
// Empty input returns the default value.
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

// Prompts for a boolean (y/n/yes/no/true/false/1/0).
// Empty input returns the default.
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

// Prompts for an integer within a valid range.
// Repeats until valid input is received.
template <typename T>
T prompt_int(std::string_view label, T default_val, std::pair<T, T> range) {
  auto [min_val, max_val] = range;
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

// Prompts for a choice from a list of valid options.
// Shows options as (opt1|opt2|opt3). Empty input returns default.
// lazy: doesn't validate against the choices list — any string is accepted.
std::string prompt_choice(std::string_view label, std::string_view default_val,
                          std::span<const std::string_view> choices) {
  std::cout << "  " << bold << label << reset << " " << dim << "[" << default_val << "]" << reset
            << " (";
  bool first = true;
  for (auto choice : choices) {
    if (!first) std::cout << "|";
    first = false;
    std::cout << choice;
  }
  std::cout << "): ";
  std::string input;
  std::getline(std::cin, input);
  return input.empty() ? std::string(default_val) : input;
}

// Interactive dependency input loop.
// Prompts for name, git URL, and tag for each dependency.
// Dependencies are stored as "name:url:tag" strings.
void prompt_dependencies(application::InitOptions& opts) {
  if (!prompt_bool("add dependencies", false)) {
    return;
  }
  std::cout << "\n  " << dim << "enter dependency info (empty name to finish)" << reset << "\n";
  while (true) {
    std::cout << "\n";
    std::string name = prompt_string("  dep name", "");
    if (name.empty()) break;

    std::string default_url = "https://github.com/" + name + "/" + name + ".git";
    std::string url = prompt_string("  git url", default_url);
    std::string tag = prompt_string("  git tag", "main");
    opts.dependencies.push_back(name + ":" + url + ":" + tag);
    std::cout << "    " << green << check << reset << " added " << name << "\n";
  }
}

}  // namespace

// Main interactive init wizard.
// Walks the user through all options step by step.
// Press enter to accept defaults, type a value to override.
// The flow is: project name → formatter → tidy → cmake → conan
void run_interactive_init(application::InitOptions& opts) {
#ifdef _WIN32
  enable_windows_ansi();
#endif
  std::cout << "\n";
  std::cout << bold << "  metis init" << reset << "\n";
  std::cout << "  " << "enter to accept the default configs" << reset << "\n\n";

  opts.project_name = prompt_string("project name", opts.project_name);

  static constexpr std::array format_style = {
      std::string_view{"google"},  std::string_view{"llvm"},   std::string_view{"chromium"},
      std::string_view{"mozilla"}, std::string_view{"webkit"}, std::string_view{"microsoft"},
      std::string_view{"gnu"}};

  opts.style = prompt_choice("formatter style", "google", format_style);

  opts.indent_width = prompt_int("indent width", opts.indent_width, std::pair{1, 16});
  opts.column_limit = prompt_int("column limit", opts.column_limit, std::pair{20, 500});

  opts.enable_clang_tidy = prompt_bool("enable clang-tidy", opts.enable_clang_tidy);

  if (opts.enable_clang_tidy) {
    static constexpr auto tidy_presets =
        std::to_array<std::string_view>({"minimal", "standard", "strict", "custom"});
    std::string preset = prompt_choice("tidy preset", "standard", tidy_presets);
    if (preset == "minimal" || preset == "standard" || preset == "strict" || preset == "custom") {
      opts.tidy_preset = preset;
    }

    static constexpr auto severity_presets =
        std::to_array<std::string_view>({"note", "warning", "error"});
    opts.tidy_severity = prompt_choice("tidy severity", "error", severity_presets);
  }

  opts.enable_cmake = prompt_bool("generate CMakeLists.txt", opts.enable_cmake);
  if (opts.enable_cmake) {
    opts.generate_source = true;
    static constexpr auto standards = std::to_array<std::string_view>({"17", "20", "23"});
    opts.cmake_cpp_standard = prompt_choice("C++ Standard", "20", standards);

    static constexpr auto targets =
        std::to_array<std::string_view>({"executable", "static", "shared", "header-only"});
    opts.cmake_target_type = prompt_choice("target type", "executable", targets);

    opts.cmake_enable_testing = prompt_bool("enable testing", opts.cmake_enable_testing);
    opts.cmake_enable_sanitizers = prompt_bool("enable sanitizers", opts.cmake_enable_sanitizers);
    prompt_dependencies(opts);
  }

  opts.enable_conan = prompt_bool("generate conanfile.py", opts.enable_conan);

  opts.enable_compiler_checks = prompt_bool("enable compiler checks", opts.enable_compiler_checks);
  if (opts.enable_compiler_checks) {
    static constexpr auto compilers =
        std::to_array<std::string_view>({"g++", "clang++", "gcc", "clang"});
    opts.compiler = prompt_choice("compiler", "g++", compilers);

    static constexpr auto standards = std::to_array<std::string_view>({"17", "20", "23", "26"});
    opts.compiler_cpp_standard = prompt_choice("C++ standard", "20", standards);

    opts.compiler_werror = prompt_bool("treat warnings as errors (-Werror)", opts.compiler_werror);

    opts.compiler_debug_and_release =
        prompt_bool("check both debug and release configurations", opts.compiler_debug_and_release);
  }

  std::cout << "\n";
}

// Prints a summary of what was created after init completes.
// Shows project name, style, and which tooling files were generated.
void print_init_summary(const application::InitOptions& opts,
                        const application::InitResult& result) {
  auto style_name = [](const std::string& s) -> std::string {
    std::string lower;
    for (char chr : s) {
      lower += static_cast<char>(std::tolower(static_cast<unsigned char>(chr)));
    }
    if (lower == "google") return "Google";
    if (lower == "llvm") return "LLVM";
    if (lower == "chromium") return "Chromium";
    if (lower == "mozilla") return "Mozilla";
    if (lower == "webkit") return "WebKit";
    if (lower == "microsoft") return "Microsoft";
    if (lower == "gnu") return "GNU";
    return s;
  };

  std::cout << "\n";
  std::cout << bold << " metis initialized " << reset << "\n";
  std::cout << "  " << dim << result.project_config_path << reset << "\n\n";

  std::cout << "  " << bold << "project" << reset << "\n";
  std::cout << "    " << bullet << " name:   " << opts.project_name << "\n";
  std::cout << "    " << bullet << " style:  " << style_name(opts.style) << "\n";

  std::cout << "\n  " << bold << "tooling" << reset << "\n";
  std::cout << "    " << green << check << reset << " .clang-format";
  if (opts.indent_width != 2 || opts.column_limit != 100) {
    std::cout << "  (indent=" << opts.indent_width << ", limit=" << opts.column_limit << ")";
  }
  std::cout << "\n";

  if (opts.enable_clang_tidy) {
    std::cout << "    " << green << check << reset << " .clang-tidy"
              << "  (preset: " << opts.tidy_preset << ", severity: " << opts.tidy_severity << ")\n";
  }

  if (!result.conan_config_path.empty()) {
    std::cout << "    " << green << check << reset << " conanfile.py\n";
  }

  std::cout << "\n  " << cyan << arrow << reset << " next: " << bold << "metis install"
            << reset << " " << dim << "to set up pre-commit hooks" << reset << "\n\n";
}

}  // namespace metis::presentation
