#include "sniffercommit/tooling_config.hpp"

#include <fmt/format.h>

#include <stdexcept>
#include <string>
#include <vector>

namespace sniffercommit::tooling {

std::string ClangFormatConfig::validate() const noexcept {
  if (ident_width < 1 || ident_width > 16) {
    return "IndentWidth must be between 1 and 16";
  }

  if (column_limit < 20 || column_limit > 500) {
    return "ColumnLimit must be between 20 and 500";
  }

  return "";
}

std::string style_name(FormatterStyle style) {
  switch (style) {
    case FormatterStyle::Google:
      return "Google";
    case FormatterStyle::LLVM:
      return "LLVM";
    case FormatterStyle::Chromium:
      return "Chromium";
    case FormatterStyle::Mozilla:
      return "Mozilla";
    case FormatterStyle::WebKit:
      return "WebKit";
    case FormatterStyle::Microsoft:
      return "Microsoft";
    case FormatterStyle::GNU:
      return "GNU";
  }
  return "Google";
}

FormatterStyle parse_style(const std::string& style) {
  std::string lower;

  for (char chr : style) {
    lower += static_cast<char>(std::tolower(static_cast<unsigned char>(chr)));
  }

  if (lower == "google") {
    return FormatterStyle::Google;
  }
  if (lower == "llvm") {
    return FormatterStyle::LLVM;
  }
  if (lower == "chromium") {
    return FormatterStyle::Chromium;
  }
  if (lower == "mozilla") {
    return FormatterStyle::Mozilla;
  }
  if (lower == "webkit") {
    return FormatterStyle::WebKit;
  }
  if (lower == "microsoft") {
    return FormatterStyle::Microsoft;
  }
  if (lower == "gnu") {
    return FormatterStyle::GNU;
  }

  throw std::runtime_error("Unknown formatter style: " + style);
}

std::string generate_clang_format(const ClangFormatConfig& cfg) {
  if (auto err = cfg.validate(); !err.empty()) {
    throw std::runtime_error("Invalid clang format config: " + err);
  }

  auto bool_str = [](bool val) { return val ? "true" : "false"; };

  return fmt::format(
      R"(---
BasedOnStyle: {}

IndentWidth: {}
ColumnLimit: {}

PointerAlignment: {}
BreakBeforeBraces: {}

Standard: {}

SortIncludes: {}
ReflowComments: {}
AlignConsecutiveAssignments: {}

...
)",
      style_name(cfg.style), cfg.ident_width, cfg.column_limit, cfg.pointer_alignment,
      cfg.break_before_braces, cfg.standard, bool_str(cfg.sort_includes),
      bool_str(cfg.reflow_comments), bool_str(cfg.align_consecutive_assignments));
}

// NOTE: clang tidy logic
std::string ClangTidyConfig::validate() const noexcept {
  if (preset == TidyPreset::Custom && checks.empty()) {
    return "Custom preset requires at least one explicit checks";
  }

  if (header_filter_level < 0 || header_filter_level > 2) {
    return "header_filter_level must be 0 (none), 1 (project), or 2 (all)";
  }

  return "";
}

std::string preset_name(TidyPreset preset) {
  switch (preset) {
    case TidyPreset::Minimal:
      return "minimal";
    case TidyPreset::Standard:
      return "standard";
    case TidyPreset::Strict:
      return "strict";
    case TidyPreset::Custom:
      return "custom";
  }

  return "standard";
}

std::string severity_name(TidySeverity severity) {
  switch (severity) {
    case TidySeverity::Note:
      return "note";
    case TidySeverity::Warning:
      return "warning";
    case TidySeverity::Error:
      return "error";
  }
  return "error";
}

std::vector<std::string> preset_checks(TidyPreset preset) {
  switch (preset) {
    case TidyPreset::Minimal:
      return {
          "cppcoreguidelines-*",
          "bugprone-*",
          "clang-analyzer-*",
          "--cppcoreguidelines-avoid-magic-numbers",
          "-cppcoreguidelines-pre-bounds-array-to-pointer-decay",
      };
    case TidyPreset::Standard:
      return {
          "cppcoreguidelines-*",
          "bugprone-*",
          "clang-analyzer-*",
          "modernize-*",
          "performance-*",
          "-cppcoreguidelines-avoid-magic-numbers",
          "-cppcoreguidelines-pro-bounds-array-to-pointer-decay",
          "-modernize-use-trailing-return-type",
      };

    case TidyPreset::Strict:
      return {
          "*",
          "-abseil-*",
          "-altera-*",
          "-fuchsia-*",
          "-llvm-*",
          "-zircon-*",
          "-google-readability-todo",
          "-readability-identifier-length",
          "-cppcoreguidelines-avoid-magic-numbers",
          "-cppcoreguidelines-pro-bounds-array-to-pointer-decay",
          "-modernize-use-trailing-return-type",
      };

    case TidyPreset::Custom:
      return {};
  }

  return preset_checks(TidyPreset::Standard);
}

std::string generate_clang_tidy(const ClangTidyConfig& cfg) {
  if (auto err = cfg.validate(); !err.empty()) {
    throw std::runtime_error("invalid clang-tidy config: " + err);
  }

  std::vector<std::string> active_checks;

  if (cfg.preset != TidyPreset::Custom) {
    active_checks = preset_checks(cfg.preset);
  } else {
    active_checks = cfg.checks;
  }

  // appending extra checks
  for (const auto& check : cfg.extra_checks) {
    active_checks.push_back(check);
  }

  for (const auto& check : cfg.exclude_checks) {
    active_checks.push_back("-" + check);
  }

  std::string check_str;
  for (size_t i = 0; i < active_checks.size(); ++i) {
    if (i > 0) {
      check_str += ",";
    }

    check_str += active_checks.at(i);
  }

  std::string header_filter;

  switch (cfg.header_filter_level) {
    case 0:
      header_filter += "\"\"";
      break;
    case 1:
    case 2:
    default:
      header_filter += "\".*\"";
      break;
  }

  auto wae_pattern = [](TidySeverity sev) -> std::string_view {
    switch (sev) {
      case TidySeverity::Note:
        return "";
      case TidySeverity::Warning:
        return "clang-diagnostic-*";
      case TidySeverity::Error:
        return "*";
    }
    return "";
  };

  return fmt::format(
      R"(---
# Generated by sniffercommit v0.2.1 | DO NOT EDIT manually
# Preset: {}
# Severity threshold: {}

Checks: "{}"
WarningsAsErrors: '{}'
HeaderFilterRegex: {}
FormatStyle: {}

CheckOptions:
  - key: readability-identifier-naming.NamespaceCase
    value: lower_case
  - key: readability-identifier-naming.ClassCase
    value: CamelCase
  - key: readability-identifier-naming.FunctionCase
    value: camelBack
  - key: readability-identifier-naming.VariableCase
    value: lower_case
  - key: readability-identifier-naming.MemberCase
    value: lower_case
  - key: readability-identifier-naming.MemberSuffix
    value: _
  - key: cppcoreguidelines-special-member-functions.AllowSoleDefaultDtor
    value: 1
  - key: cppcoreguidelines-avoid-magic-numbers.IgnorePowersOf2
    value: 1
  - key: modernize-loop-convert.MaxCopySize
    value: 16
  - key: modernize-pass-by-value.IncludeStyle
    value: llvm
  - key: modernize-use-default-member-init.UseAssignment
    value: 1
  - key: performance-unnecessary-value-param.AllowedTypes
    value: "std::.*;llvm::.*;fmt::.*"
  - key: bugprone-exception-escape.FunctionThatShouldNotThrow
    value: "main;wmain;WinMain;wWinMain"
  - key: readability-function-cognitive-complexity.Threshold
    value: 25
  - key: readability-function-cognitive-complexity.DescribeBasicIncrements
    value: 0

...
)",
      preset_name(cfg.preset), severity_name(cfg.warnings_as_errors), check_str,
      wae_pattern(cfg.warnings_as_errors), header_filter, cfg.format_style ? "file" : "none");
}

std::string CMakeConfig::validate() const noexcept {
  if (project_name.empty()) {
    return "Project name cannot be empty";
  }

  if (version.empty()) {
    return "Version cannot be empty";
  }

  if (target_type != TargetType::HeaderOnly && source_files.empty()) {
    return "At least one source file is required for non-header-only targets";
  }

  return "";
}

std::string cpp_standard_name(CppStandard cppStandard) {
  switch (cppStandard) {
    case sniffercommit::tooling::CppStandard::Cpp17:
      return "17";
    case sniffercommit::tooling::CppStandard::Cpp20:
      return "20";
    case sniffercommit::tooling::CppStandard::Cpp23:
      return "23";
  }
  return "20";
}

std::string build_preset_name(BuildTypePreset preset) {
  switch (preset) {
    case BuildTypePreset::ReleaseOnly:
      return "Release";
    case BuildTypePreset::DebugRelease:
      return "Debug;Release";
    case BuildTypePreset::Full:
      return "Debug;Release;RelWithDebInfo;MinSizeRel";
  }

  return "Debug;Release";
}

std::string depedency_strategy_name(DepedencyStrategy strategy) {
  switch (strategy) {
    case sniffercommit::tooling::DepedencyStrategy::FetchContent:
      return "FetchContent";
    case sniffercommit::tooling::DepedencyStrategy::FindPackage:
      return "find_package";
    case sniffercommit::tooling::DepedencyStrategy::Conan:
      return "Conan";
    case sniffercommit::tooling::DepedencyStrategy::Vcpkg:
      return "vcpkg";
  }
  return "FetchContent";
}

std::string target_type_name(TargetType type) {
  switch (type) {
    case TargetType::Executable:
      return "EXECUTABLE";
    case TargetType::StaticLibrary:
      return "STATIC";
    case TargetType::SharedLibrary:
      return "SHARED";
    case TargetType::HeaderOnly:
      return "INTERFACE";
  }
  return "EXECUTABLE";
}

// CMakeLists generate
std::string generate_cmake_lists(const CMakeConfig& cfg) {
  if (auto err = cfg.validate(); !err.empty()) {
    throw std::runtime_error("Invalid CMake config: " + err);
  }

  std::string target_name = cfg.target_name.empty() ? cfg.project_name : cfg.target_name;
  std::string cmake;

  // Header
  cmake += fmt::format(
      "# Generated by sniffercommit v0.2.1 | DO NOT EDIT MANUALLY\n"
      "# Project: {} v{}\n"
      "# C++ standard: {}\n"
      "# Build Preset: {}\n\n",
      cfg.project_name, cfg.version, cpp_standard_name(cfg.cpp_standard),
      build_preset_name(cfg.build_preset));

  cmake += "cmake_minimum_required(VERSION 3.20)\n";

  // Project declaration
  if (!cfg.description.empty()) {
    cmake += fmt::format("project({}\n", cfg.project_name);
    cmake += fmt::format("  VERSION {}\n", cfg.version);
    cmake += fmt::format("  DESCRIPTION \"{}\"\n", cfg.description);
    if (!cfg.homepage_url.empty()) {
      cmake += fmt::format("  HOMEPAGE_URL \"{}\"\n", cfg.homepage_url);
    }
    cmake += "  LANGUAGES CXX\n)\n\n";
  } else {
    cmake += fmt::format("project({} VERSION {} LANGUAGES CXX)\n\n", cfg.project_name, cfg.version);
  }

  // C++ standard
  cmake += fmt::format("set(CMAKE_CXX_STANDARD {})\n", cpp_standard_name(cfg.cpp_standard));
  cmake += fmt::format("set(CMAKE_CXX_STANDARD_REQUIRED {})\n",
                       cfg.cpp_standard_required ? "ON" : "OFF");
  cmake += fmt::format("set(CMAKE_CXX_EXTENSIONS {})\n", cfg.enable_extension ? "ON" : "OFF");
  cmake += "\n";

  // Build type preset
  if (cfg.build_preset != BuildTypePreset::ReleaseOnly) {
    cmake += fmt::format(
        "# Build type configuration\n"
        "set(CMAKE_CONFIGURATION_TYPES \"{}\" CACHE STRING \"Available build types\" FORCE)\n"
        "if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)\n"
        "  set(CMAKE_BUILD_TYPE Release CACHE STRING \"Build type\" FORCE)\n"
        "endif()\n\n",
        build_preset_name(cfg.build_preset));
  }

  if (cfg.export_compile_commands) {
    cmake += "set(CMAKE_EXPORT_COMPILE_COMMANDS ON)\n\n";
  }

  // IPO/LTO
  if (cfg.enable_ipo) {
    cmake += "include(CheckIPOSupported)\n";
    cmake += "check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)\n";
    cmake += "if(ipo_supported)\n";
    cmake += "  set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)\n";
    cmake += "endif()\n\n";
  }

  // Dependencies (fixed spelling and logic)
  if (!cfg.depedencies.empty()) {
    cmake += "# Dependencies\n";
    switch (cfg.dep_strategy) {
      case sniffercommit::tooling::DepedencyStrategy::FetchContent:
        cmake += "include(FetchContent)\n";
        for (const auto& dep : cfg.depedencies) {
          cmake += fmt::format(
              "FetchContent_Declare(\n"
              "  {}\n"
              "  GIT_REPOSITORY https://github.com/example/{}.git\n"
              "  GIT_TAG main\n"
              "  GIT_SHALLOW ON\n"
              ")\n",
              dep, dep);
        }
        cmake += "FetchContent_MakeAvailable(";
        for (size_t i = 0; i < cfg.depedencies.size(); ++i) {
          if (i > 0) {
            cmake += " ";
          }
          cmake += cfg.depedencies[i];
        }
        cmake += ")\n\n";
        break;

      case sniffercommit::tooling::DepedencyStrategy::FindPackage:
        for (const auto& dep : cfg.depedencies) {
          cmake += fmt::format("find_package({} REQUIRED)\n", dep);
        }
        cmake += "\n";
        break;

      case sniffercommit::tooling::DepedencyStrategy::Conan:
        cmake += "# Conan integration\n";
        cmake += "list(APPEND CMAKE_PREFIX_PATH \"${CMAKE_BINARY_DIR}/generators\")\n";
        cmake += "find_package(fmt REQUIRED)\n\n";
        break;

      case sniffercommit::tooling::DepedencyStrategy::Vcpkg:
        cmake += "# vcpkg integration\n";
        cmake += "# ensure VCPKG_ROOT is set in environment\n";
        cmake += "find_package(fmt CONFIG REQUIRED)\n\n";
        break;
    }
  }

  cmake += "# Source files\n";
  if (cfg.source_files.empty()) {
    cmake += "file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS \"src/*.cpp\" \"src/*.cc\")\n";
  } else {
    cmake += "set(SOURCES\n";
    for (const auto& file : cfg.source_files) {
      cmake += fmt::format("  {}\n", file);
    }
    cmake += ")\n";
  }
  cmake += "\n";

  if (!cfg.header_files.empty()) {
    cmake += "set(HEADERS\n";
    for (const auto& file : cfg.header_files) {
      cmake += fmt::format("  {}\n", file);
    }
    cmake += ")\n\n";
  }

  cmake += "# Target definition\n";
  if (cfg.target_type == TargetType::HeaderOnly) {
    cmake += fmt::format("add_library({} INTERFACE)\n", target_name);
    cmake += fmt::format("target_include_directories({} INTERFACE\n", target_name);
    if (cfg.include_dirs.empty()) {
      cmake += "  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>\n";
      cmake += "  $<INSTALL_INTERFACE:include>\n";
    } else {
      for (const auto& dir : cfg.include_dirs) {
        cmake += fmt::format("  {}\n", dir);
      }
    }
    cmake += ")\n";
  } else {
    if (cfg.target_type == TargetType::Executable) {
      cmake += fmt::format("add_executable({} ${{SOURCES}})\n", target_name);
    } else {
      cmake += fmt::format("add_library({} {} ${{SOURCES}})\n", target_name,
                           target_type_name(cfg.target_type));
    }

    if (!cfg.include_dirs.empty() || cfg.target_type != TargetType::HeaderOnly) {
      cmake += fmt::format("target_include_directories({} PUBLIC\n", target_name);
      if (cfg.include_dirs.empty()) {
        cmake += "  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>\n";
        cmake += "  $<INSTALL_INTERFACE:include>\n";
      } else {
        for (const auto& dir : cfg.include_dirs) {
          cmake += fmt::format("  {}\n", dir);
        }
      }
      cmake += ")\n";
    }
  }
  cmake += "\n";

  if (!cfg.depedencies.empty() && cfg.target_type != TargetType::HeaderOnly) {
    cmake += fmt::format("target_link_libraries({} PUBLIC\n", target_name);
    for (const auto& dep : cfg.depedencies) {
      cmake += fmt::format("  {}::{}\n", dep, dep);
    }
    cmake += ")\n\n";
  }

  if (cfg.enable_warnings && cfg.target_type != TargetType::HeaderOnly) {
    cmake += "# Compiler warnings\n";
    cmake += "if(CMAKE_CXX_COMPILER_ID MATCHES \"GNU|Clang\")\n";
    cmake += fmt::format("  target_compile_options({} PRIVATE\n", target_name);
    cmake += "    -Wall -Wextra -Wpedantic\n";
    cmake += "    -Wconversion -Wsign-conversion\n";
    cmake += "    -Wshadow -Wnon-virtual-dtor\n";
    if (cfg.warnings_as_errors) {
      cmake += "    -Werror\n";
    }
    cmake += "  )\n";
    cmake += "elseif(MSVC)\n";
    cmake += fmt::format("  target_compile_options({} PRIVATE /W4", target_name);
    if (cfg.warnings_as_errors) {
      cmake += " /WX";
    }
    cmake += ")\n";
    cmake += "endif()\n\n";
  }

  if (cfg.enable_sanitizers && cfg.target_type != TargetType::HeaderOnly) {
    cmake += "# Sanitizers (Debug only)\n";
    cmake += "if(CMAKE_CXX_COMPILER_ID MATCHES \"GNU|Clang\")\n";
    cmake += fmt::format(
        "  target_compile_options({} PRIVATE "
        "\"$<$<CONFIG:Debug>:-fsanitize=address,undefined>\")\n",
        target_name);
    cmake += fmt::format(
        "  target_link_options({} PRIVATE \"$<$<CONFIG:Debug>:-fsanitize=address,undefined>\")\n",
        target_name);
    cmake += "endif()\n\n";
  }

  if (cfg.enable_clang_tidy && cfg.target_type != TargetType::HeaderOnly) {
    cmake += "# clang-tidy integration\n";
    cmake += "find_program(CLANG_TIDY_EXE NAMES clang-tidy)\n";
    cmake += "if(CLANG_TIDY_EXE)\n";
    cmake += fmt::format("  set_target_properties({} PROPERTIES\n", target_name);
    cmake +=
        "    CXX_CLANG_TIDY "
        "\"${CLANG_TIDY_EXE};--config-file=${CMAKE_CURRENT_SOURCE_DIR}/.clang-tidy\"\n";
    cmake += "  )\n";
    cmake += "endif()\n\n";
  }

  if (cfg.enable_clang_format) {
    cmake += "# clang-format integration\n";
    cmake += "find_program(CLANG_FORMAT_EXE NAMES clang-format)\n";
    cmake += "if(CLANG_FORMAT_EXE)\n";
    cmake += "  file(GLOB_RECURSE ALL_SOURCES src/*.cpp src/*.hpp include/*.hpp)\n";
    cmake += "  add_custom_target(format\n";
    cmake += "    COMMAND ${CLANG_FORMAT_EXE} -i ${ALL_SOURCES}\n";
    cmake += "    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}\n";
    cmake += "    COMMENT \"Running clang-format\"\n";
    cmake += "  )\n";
    cmake += "endif()\n\n";
  }

  if (cfg.enable_testing) {
    cmake += "# Testing\n";
    cmake += "enable_testing()\n";
    cmake += "add_subdirectory(tests)\n\n";
  }

  if (cfg.enable_install && cfg.target_type != TargetType::HeaderOnly) {
    cmake += "# Installation\n";
    cmake += fmt::format("install(TARGETS {}\n", target_name);
    cmake += "  EXPORT ${PROJECT_NAME}Targets\n";
    cmake += "  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}\n";
    cmake += "  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}\n";
    cmake += "  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}\n";
    cmake += ")\n\n";
  }

  return cmake;
}

// sniffercommit like projects
std::string generate_cmake_lists_default(const std::string& project_name, CppStandard cpp_std) {
  CMakeConfig cfg;

  cfg.project_name = project_name;
  cfg.version = "0.2.1";
  cfg.cpp_standard = cpp_std;
  cfg.target_name = project_name;
  cfg.source_files = {"src/main.cpp"};
  cfg.include_dirs = {"${CMAKE_CURRENT_SOURCE_DIR}/include"};
  cfg.enable_warnings = true;
  cfg.enable_install = true;
  cfg.export_compile_commands = true;
  return generate_cmake_lists(cfg);
}

}  // namespace sniffercommit::tooling
