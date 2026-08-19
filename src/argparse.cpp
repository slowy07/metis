#include "sniffercommit/argparse.hpp"

#include <algorithm>
#include <iostream>
#include <iterator>
#include <string>

namespace sniffercommit {

ArgParser::ArgParser(std::string_view name, std::string_view desc)
    : app_name_(name), description_(desc) {}

// Registers a boolean flag (no value).
// When the flag is present, storage is set to true.
ArgParser& ArgParser::add_flag(std::string_view short_flag, std::string_view long_flag,
                               std::string_view desc, bool& storage) {
  options_.emplace_back(Option{.short_flag = short_flag,
                               .long_flag = long_flag,
                               .description = desc,
                               .default_value = "false",
                               .has_value = false});
  flag_stores_.emplace_back(&storage);
  return *this;
}

ArgParser& ArgParser::add_subcommand(std::string_view name, std::string_view desc) {
  subcommands_.emplace_back(
      Subcommand{.name = std::string(name), .description = std::string(desc)});
  return *this;
}

ArgParser& ArgParser::set_version(std::string_view ver) {
  version_ = ver;
  return *this;
}

// Parses command-line arguments.
//
// Flow:
// 1. Check for --help/-h and --version/-v (early exit)
// 2. If subcommands are registered, match the first positional arg
// 3. Match remaining args against registered options/flags
//
// Returns true if a subcommand was matched, false for help/version/unknown.
// lazy: doesn't support combined short flags (-abc) or --key=value syntax.
// These haven't been needed.
bool ArgParser::parse(int argc, char** argv) {
  auto argc_sz = static_cast<size_t>(argc);
  if (argc_sz < 2) {
    show_help();
    return false;
  }
  args_ = std::span(argv, argc_sz);

  for (size_t i = 1; i < argc_sz; ++i) {
    std::string_view arg = args_.data()[i];
    if (arg == "--help" || arg == "-h") {
      show_help();
      return false;
    }
    if (!version_.empty() && (arg == "--version" || arg == "-v")) {
      std::cout << app_name_ << " " << version_ << "\n";
      return false;
    }
  }

  if (!subcommands_.empty()) {
    std::string_view first_arg = args_.data()[1];
    if (!first_arg.starts_with("-")) {
      auto found = std::ranges::find_if(
          subcommands_, [first_arg](const auto& cmd) { return cmd.name == first_arg; });

      if (found != subcommands_.end()) {
        active_subcommand_ = found->name;
        return true;
      }
      std::cerr << "[ERROR] Unknown subcommand: " << first_arg << "\n\n";
      show_help();
      return false;
    }
  }

  for (size_t i = 0; i < args_.size(); ++i) {
    std::string_view arg = args_.data()[i];
    if (!arg.starts_with('-')) {
      continue;
    }

    auto opt_it = std::ranges::find_if(options_, [arg](const Option& opt) {
      return opt.short_flag == arg || opt.long_flag == arg;
    });

    if (opt_it == options_.end()) {
      if (!active_subcommand_.empty()) {
        continue;
      }
      std::cerr << "[ERROR] Unknown option: " << arg << "\n\n";
      show_help();
      return false;
    }

    if (opt_it->has_value) {
      if (i + 1 >= args_.size()) {
        std::cerr << "[ERROR] Option " << arg << " requires value\n";
        return false;
      }
      std::string value = std::string(args_.data()[++i]);
      auto opt_idx = static_cast<size_t>(std::distance(options_.begin(), opt_it));
      if (opt_idx < option_stores_.size()) {
        option_stores_.at(opt_idx)(value);
      }
    } else {
      auto opt_idx = static_cast<size_t>(std::distance(options_.begin(), opt_it));
      if (opt_idx < flag_stores_.size() && flag_stores_.at(opt_idx) != nullptr) {
        *flag_stores_.at(opt_idx) = true;
      }
    }
  }

  return true;
}

std::string_view ArgParser::get_subcommand() const { return active_subcommand_; }

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void ArgParser::print_aligned(std::string_view left, std::string_view right) {
  std::cout << "  " << left;

  if (left.size() < k_padding) {
    std::cout << std::string(k_padding - left.size(), ' ');
  } else {
    std::cout << "\n";
    std::cout << std::string(k_padding + 2, ' ');
  }

  std::cout << right << "\n";
}

void ArgParser::print_section_title(std::string_view title) { std::cout << title << ":\n"; }

// Prints the help message with formatted output.
// Sections: Core Workflow (subcommands), Examples, Subcommand details, Global Options.
// Uses column alignment for readability.
void ArgParser::show_help() const {
  std::cout << app_name_ << " - " << description_ << "\n\n";

  std::cout << "Usage:\n";

  std::cout << "  " << app_name_ << " [OPTIONS] <SUBCOMMAND> [ARGS]\n\n";

  print_section_title("Core Workflow");

  for (const auto& cmd : subcommands_) {
    print_aligned(cmd.name, cmd.description);
  }

  std::cout << "\n";

  print_section_title("Examples");
  std::cout << "  " << app_name_ << " init\n";
  std::cout << "  " << app_name_ << " init --style llvm\n";
  std::cout << "  " << app_name_ << " init --name ultra-slowy\n";
  std::cout << "  " << app_name_ << " init --enable-clang-tidy\n";
  std::cout << "  " << app_name_ << " init --enable-cmake\n";
  std::cout << "  " << app_name_ << " install\n";
  std::cout << "  " << app_name_ << " run --all-files\n";
  std::cout << "  " << app_name_ << " run src/main.cpp\n";
  std::cout << "  " << app_name_
            << " generate-gha > "
               ".github/workflows/sniffercommit.yml\n";
  std::cout << "  " << app_name_ << " install-compiler --compiler gcc --cpp-standard 20\n";
  std::cout << "  " << app_name_ << " test --coverage\n\n";

  print_section_title("Subcommands");

  std::cout << "  init\n";

  std::cout << "      Create:\n";

  std::cout << "        - .sniffercommit.toml\n";

  std::cout << "        - .clang-format\n";

  std::cout << "        - [--enable-clang-tidy] .clang-tidy\n";

  std::cout << "        - [--enable-cmake]     CMakeLists.txt + src/main.cpp\n";

  std::cout << "        - [--enable-conan]     conanfile.py\n";

  std::cout << "        - [--generate-src]     src/main.cpp\n\n";

  std::cout << "      Options:\n";

  std::cout << "        --style "
               "<google|llvm|chromium|mozilla|webkit|microsoft|gnu>\n";

  std::cout << "        --name <project-name>\n";

  std::cout << "        --indent-width <n>\n";

  std::cout << "        --column-limit <n>\n";

  std::cout << "        --pointer-alignment "
               "<Left|Right|Middle>\n";

  std::cout << "        --brace-style <Attach|Allman|...>\n";

  std::cout << "        --enable-clang-tidy, --tidy\n";

  std::cout << "        --tidy-preset "
               "<minimal|standard|strict|custom>\n";

  std::cout << "        --tidy-severity "
               "<note|warning|error>\n";

  std::cout << "        --tidy-header-filter <0|1|2>\n";

  std::cout << "        --enable-cmake, --cmake\n";

  std::cout << "        --enable-conan\n";

  std::cout << "        --cmake-cpp-standard "
               "<17|20|23>\n";

  std::cout << "        --cmake-target-type "
               "<executable|static|shared|header-only>\n";

  std::cout << "        --cmake-enable-testing\n";

  std::cout << "        --cmake-enable-sanitizers\n";

  std::cout << "        --generate-src\n\n";

  std::cout << "  install\n";

  std::cout << "      Generate and install:\n";

  std::cout << "        .git/hooks/pre-commit\n\n";

  std::cout << "  run\n";

  std::cout << "      Execute configured checks.\n\n";

  std::cout << "      Modes:\n";

  std::cout << "        --all-files\n";

  std::cout << "        --staged\n";

  std::cout << "        --detail\n";

  std::cout << "        <explicit files>\n\n";

  std::cout << "  generate-gha\n";

  std::cout << "      Generate production-grade "
               "GitHub Actions workflow.\n\n";

  std::cout << "  generate-gitlab\n";

  std::cout << "      Generate GitLab CI workflow.\n\n";

  std::cout << "  install-compiler\n";

  std::cout << "      Download and install a C++ toolchain.\n\n";

  std::cout << "      Options:\n";

  std::cout << "        --compiler <gcc|clang>         [default: gcc]\n";

  std::cout << "        --version <version>\n";

  std::cout << "        --cpp-standard <17|20|23|26>   [default: 20]\n";

  std::cout << "        --prefix <path>\n";

  std::cout << "        --force\n";

  std::cout << "        --dry-run, -n\n\n";

  std::cout << "  test\n";

  std::cout << "      Run ctest and optional coverage checks.\n\n";

  std::cout << "      Options:\n";

  std::cout << "        --coverage\n";

  std::cout << "        --verbose, -V\n";

  std::cout << "        <build-dir>                   [default: build]\n\n";

  std::cout << "  sanitizer\n";

  std::cout << "      Run sanitizer checks (ASan, UBSan, TSan, LSan).\n";

  std::cout << "      Usage:\n";

  std::cout << "        sniffercommit sanitizer\n";

  std::cout << "        sniffercommit sanitizer --verbose\n\n";

  print_section_title("Global Options");

  for (const auto& opt : options_) {
    std::string left;

    if (!opt.short_flag.empty()) {
      left += std::string(opt.short_flag);
      left += ", ";
    }

    left += std::string(opt.long_flag);

    if (opt.has_value) {
      left += " <value>";
    }

    std::string desc = std::string(opt.description);

    if (!opt.default_value.empty() && opt.has_value) {
      desc += " [default: " + opt.default_value + "]";
    }

    print_aligned(left, desc);
  }

  if (!version_.empty()) {
    print_aligned("-v, --version", "Show version");
  }

  print_aligned("-h, --help", "Show help message");
}

}  // namespace sniffercommit
