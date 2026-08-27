#include "metis/argparse.hpp"

#include <algorithm>
#include <iostream>
#include <string>
#include <string_view>

#include "metis/presentation/console.hpp"

namespace metis {

ArgParser::ArgParser(std::string_view name, std::string_view desc)
  : app_name_(name)
  , description_(desc) {}

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
      Subcommand{.name = std::string(name), .description = std::string(desc), .help_text = ""});
  return *this;
}

ArgParser& ArgParser::set_subcommand_help(std::string_view name, std::string_view help_text) {
  auto it =
      std::ranges::find_if(subcommands_, [name](const auto& cmd) { return cmd.name == name; });

  if (it != subcommands_.end()) {
    it->help_text = std::string(help_text);
  }

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
// Scans for --help/-h/--version/-v before anything else.
bool ArgParser::check_early_exit() {
  if (args_.size() >= 3) {
    std::string_view first = args_[1];
    auto found =
        std::ranges::find_if(subcommands_, [first](const auto& cmd) { return cmd.name == first; });

    if (found != subcommands_.end()) {
      for (size_t i = 2; i < args_.size(); ++i) {
        std::string_view arg = args_[i];
        if (arg == "--help" || arg == "-h") {
          show_subcommand_help(found->name);
          return true;
        }
      }
    }
  }

  for (size_t i = 1; i < args_.size(); ++i) {
    std::string_view arg = args_[i];

    if (arg == "--help" || arg == "-h") {
      show_help();
      return true;
    }

    if (!version_.empty() && (arg == "--version" || arg == "-v")) {
      std::cout << app_name_ << " " << version_ << "\n";
      return true;
    }
  }

  return false;
}

// First positional argument selects a subcommand when one matches; unknown
// positionals are rejected here so later loops only see options.
bool ArgParser::try_subcommand() {
  if (subcommands_.empty() || args_.size() < 2 || std::string_view(args_[1]).starts_with("-")) {
    return false;
  }
  std::string_view first_arg = args_[1];
  auto found = std::ranges::find_if(subcommands_,
                                    [first_arg](const auto& cmd) { return cmd.name == first_arg; });

  if (found == subcommands_.end()) {
    std::cerr << "[ERROR] Unknown subcommand: " << first_arg << "\n\n";
    show_help();
    return true;
  }
  active_subcommand_ = found->name;
  return true;
}

// Applies the option at index i (value store or flag store), consuming its
// value argument when required. Unknown options are tolerated while a
// subcommand is active — those own their flags.
bool ArgParser::apply_option(size_t& i) {
  std::string_view arg = args_[i];
  auto opt_it = std::ranges::find_if(
      options_, [arg](const Option& opt) { return opt.short_flag == arg || opt.long_flag == arg; });

  if (opt_it == options_.end()) {
    if (!active_subcommand_.empty()) {
      return true;
    }
    std::cerr << "[ERROR] Unknown option: " << arg << "\n\n";
    show_help();
    return false;
  }

  if (!opt_it->has_value) {
    if (opt_it->store_index < flag_stores_.size() &&
        flag_stores_.at(opt_it->store_index) != nullptr) {
      *flag_stores_.at(opt_it->store_index) = true;
    }
    return true;
  }

  if (i + 1 >= args_.size()) {
    std::cerr << "[ERROR] Option " << arg << " requires value\n";
    return false;
  }
  if (opt_it->store_index < option_stores_.size()) {
    option_stores_.at(opt_it->store_index)(std::string(args_[++i]));
  }
  return true;
}

bool ArgParser::parse(int argc, char** argv) {
  auto argc_sz = static_cast<size_t>(argc);
  if (argc_sz < 2) {
    show_help();
    return false;
  }
  args_ = std::span(argv, argc_sz);

  if (check_early_exit()) {
    return false;
  }
  if (try_subcommand()) {
    // Unknown subcommand already reported: reject unless one actually matched.
    return !active_subcommand_.empty();
  }

  for (size_t i = 0; i < args_.size(); ++i) {
    if (!std::string_view(args_[i]).starts_with('-')) {
      continue;
    }
    if (!apply_option(i)) {
      return false;
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
    std::string name_colored = presentation::Console::green(cmd.name);
    std::cout << "  " << name_colored;
    std::size_t visible_width = cmd.name.size();

    if (visible_width < k_padding) {
      std::cout << std::string(k_padding - visible_width, ' ');
    } else {
      std::cout << "\n";
      std::cout << std::string(k_padding + 2, ' ');
    }

    std::cout << presentation::Console::dim(cmd.description) << "\n";
  }

  std::cout << "\n";
  print_section_title("Examples");
  std::cout << "  " << app_name_ << " init\n";
  std::cout << "  " << app_name_ << " init --style llvm\n";
  std::cout << "  " << app_name_ << " run --all-files\n";
  std::cout << "  " << app_name_ << " run src/main.cpp\n";
  std::cout << "  " << app_name_ << " test --coverage\n";
  std::cout << "  " << app_name_ << " build --clean --jobs 8\n";
  std::cout << "  " << app_name_ << " generate-gha > .github/workflows/metis.yml\n\n";

  std::cout << "Run '" << app_name_
            << " <command> --help' for detailed usage and configuration.\n\n";

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

void ArgParser::show_subcommand_help(std::string_view name) const {
  auto it =
      std::ranges::find_if(subcommands_, [name](const auto& cmd) { return cmd.name == name; });
  if (it == subcommands_.end()) {
    return;
  }

  std::cout << app_name_ << " " << presentation::Console::green(name) << " — " << it->description
            << "\n\n";

  if (!it->help_text.empty()) {
    std::cout << it->help_text << "\n";
  }

  std::cout << "Global Options:\n";
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
  print_aligned("-h, --help", "Show this help message");
}

}  // namespace metis
