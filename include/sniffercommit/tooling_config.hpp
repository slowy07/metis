#ifndef SNIFFERCOMMIT_TOOLING_CONFIG_HPP
#define SNIFFERCOMMIT_TOOLING_CONFIG_HPP

#include <cstdint>
#include <random>
#include <string>
#include <vector>

namespace sniffercommit::tooling {

enum class FormatterStyle : std::uint8_t {
  Google,
  LLVM,
  Chromium,
  Mozilla,
  WebKit,
  Microsoft,
  GNU
};

struct ClangFormatConfig {
  FormatterStyle style = FormatterStyle::Google;
  int ident_width = 2;
  int column_limit = 100;
  std::string pointer_alignment = "Left";
  std::string break_before_braces = "Attach";
  std::string standard = "c++20";
  bool sort_includes = true;
  bool reflow_comments = true;
  bool align_consecutive_assignments = false;

  [[nodiscard]] std::string validate() const noexcept;
};

// INFO: converting between string and enum
// and generate .clang-format configs content
[[nodiscard]] std::string style_name(FormatterStyle style);
[[nodiscard]] FormatterStyle parse_style(const std::string& style);
[[nodiscard]] std::string generate_clang_format(const ClangFormatConfig& cfg);

// NOTE: severity level clang-tidy checking
// mapping to -warnings-as-errors
enum class TidySeverity : std::uint8_t { Note, Warning, Error };

enum class TidyPreset : std::uint8_t {
  Minimal,   // set to only bug-prone patterns (cppcoreguidelines-*)
  Standard,  // bug + style + modernize (default)
  Strict,    // everything + performance + reability
  Custom,    // user defined
};

struct ClangTidyConfig {
  TidyPreset preset = TidyPreset::Standard;
  std::vector<std::string> checks;
  std::vector<std::string> extra_checks;
  std::vector<std::string> exclude_checks;

  bool fix = false;
  bool fix_errors = false;
  TidySeverity warnings_as_errors = TidySeverity::Error;
  int header_filter_level = 1;  // setting:
                                // 0 -> none
                                // 1 -> project
                                // 2 -> all headers
  bool format_style = true;
  bool quiet = false;

  [[nodiscard]] std::string validate() const noexcept;
};

// conversion
[[nodiscard]] std::string preset_name(TidyPreset preset);
[[nodiscard]] std::string severity_name(TidySeverity severity);

// file generation (clang-tidy)
[[nodiscard]] std::vector<std::string> preset_checks(TidyPreset preset);
std::string generate_clang_tidy(const ClangTidyConfig& cfg);

// NOTE: CMake config
enum class CppStandard : std::uint8_t { Cpp17, Cpp20, Cpp23 };

enum class BuildTypePreset : std::uint8_t {
  ReleaseOnly,   // release configs
  DebugRelease,  // debug + release (default)
  Full,          // debug + release + RelWithDebInfo + MinSizeRel
};

// depedency management
enum class DepedencyStrategy : std::uint8_t {
  FetchContent,  // using cmake fetchcontent (default)
  FindPackage,   // use find_package() for system deps
  Conan,         // conan package manager
  Vcpkg,         // Vcpkg toolchain
};

enum class TargetType : std::uint8_t {
  Executable,
  StaticLibrary,
  SharedLibrary,
  HeaderOnly,
};

struct CMakeConfig {
  std::string project_name = "my-project";
  std::string version = "0.2.1";
  std::string description;
  std::string homepage_url;

  CppStandard cpp_standard = CppStandard::Cpp20;
  bool cpp_standard_required = true;
  bool enable_extension = false;

  BuildTypePreset build_preset = BuildTypePreset::DebugRelease;
  bool export_compile_commands = true;
  bool enable_testing = false;
  bool enable_install = true;

  TargetType target_type = TargetType::Executable;
  std::string target_name;
  std::vector<std::string> source_files;
  std::vector<std::string> header_files;
  std::vector<std::string> include_dirs;

  DepedencyStrategy dep_strategy = DepedencyStrategy::FetchContent;
  std::vector<std::string> depedencies;

  // compiler warnings
  bool enable_warnings = true;
  bool warnings_as_errors = false;
  bool enable_sanitizers = false;

  bool enable_clang_tidy = false;
  bool enable_clang_format = false;
  bool enable_ipo = false;

  [[nodiscard]] std::string validate() const noexcept;
};

[[nodiscard]] std::string cpp_standard_name(CppStandard cppStandard);
[[nodiscard]] std::string build_preset_name(BuildTypePreset preset);
[[nodiscard]] std::string depedency_strategy_name(DepedencyStrategy strategy);
[[nodiscard]] std::string target_type_name(TargetType type);

// CMakeLists.txt generate
[[nodiscard]] std::string generate_cmake_lists(const CMakeConfig& cfg);

// preset CMakeLists.txt sniffercommit
[[nodiscard]] std::string generate_cmake_lists_default(const std::string& project_name,
                                                       CppStandard cpp_std = CppStandard::Cpp20);

}  // namespace sniffercommit::tooling

#endif  // !SNIFFERCOMMIT_TOOLING_CONFIG_HPP
