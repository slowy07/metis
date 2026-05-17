#ifndef SNIFFERCOMMIT_ARGPARSER_HPP
#define SNIFFERCOMMIT_ARGPARSER_HPP

#include <algorithm>
#include <concepts>
#include <functional>
#include <iostream>
#include <iterator>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace sniffercommit {

template <typename T>
concept Parsable = std::assignable_from<T&, T> && std::default_initializable<T>;

struct Option {
  std::string_view short_flag;
  std::string_view long_flag;
  std::string_view description;
  std::string default_value;
  bool has_value = true;
};

class ArgParser {
 public:
  ArgParser(std::string_view app_name, std::string_view description)
      : app_name_(app_name), description_(description) {}

  template <Parsable T>
  ArgParser& add_option(std::string_view short_flag, std::string_view long_flag,
                        std::string_view desc, T& storage, T default_val = {}) {
    std::string default_str;
    if constexpr (std::is_same_v<T, std::string>)
      default_str = default_val;
    else
      default_str = std::to_string(default_val);
    options_.emplace_back(Option{short_flag, long_flag, desc, std::move(default_str), true});
    option_stores_.emplace_back([this, &storage](const std::string& val) {
      if constexpr (std::is_same_v<T, bool>) {
        storage = (val == "true" || val == "1");
      } else if constexpr (std::is_same_v<T, int>) {
        storage = std::stoi(val);
      } else {
        storage = val;
      }
    });
    return *this;
  }

  ArgParser& add_flag(std::string_view short_flag, std::string_view long_flag,
                      std::string_view desc, bool& storage) {
    options_.emplace_back(Option{short_flag, long_flag, desc, "false", false});
    flag_stores_.emplace_back(&storage);
    return *this;
  }

  ArgParser& add_subcommand(std::string_view name, std::string_view desc) {
    subcommands_.push_back({std::string(name), std::string(desc)});
    return *this;
  }

  ArgParser& set_version(std::string_view version) {
    version_ = version;
    return *this;
  }

  bool parse(int argc, char** argv) {
    if (argc < 2) {
      show_help();
      return false;
    }
    args_ = {argv, argv + argc};

    for (int i = 1; i < argc; ++i) {
      std::string_view arg = argv[i];
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
      std::string_view first_arg = args_[1];
      if (!first_arg.starts_with("-")) {
        auto it = std::ranges::find_if(
            subcommands_, [first_arg](const auto& cmd) { return cmd.name == first_arg; });

        if (it != subcommands_.end()) {
          active_subcommand_ = it->name;
          return true;
        } else {
          std::cerr << "[ERROR] Unknown subcommand: " << first_arg << "\n\n";
          show_help();
          return false;
        }
      }
    }

    for (size_t i = 0; i < args_.size(); ++i) {
      std::string_view arg = args_[i];
      if (!arg.starts_with('-')) continue;

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
        std::string value = std::string(args_[++i]);
        auto idx = static_cast<size_t>(std::distance(options_.begin(), opt_it));
        if (idx < option_stores_.size()) {
          option_stores_[idx](value);
        }
      } else {
        auto idx = static_cast<size_t>(std::distance(options_.begin(), opt_it));
        if (idx < flag_stores_.size() && flag_stores_[idx]) {
          *flag_stores_[idx] = true;
        }
      }
    }

    return true;
  }

  [[nodiscard]] std::string_view get_subcommand() const { return active_subcommand_; }

  void print_aligned(std::string_view left, std::string_view right) const {
    std::cout << "  " << left;

    if (left.size() < k_padding) {
      std::cout << std::string(k_padding - left.size(), ' ');
    } else {
      std::cout << "\n";
      std::cout << std::string(k_padding + 2, ' ');
    }

    std::cout << right << "\n";
  }

  void print_section_title(std::string_view title) const { std::cout << title << ":\n"; }

  void show_help() const {
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
    std::cout << "  " << app_name_ << " install\n";
    std::cout << "  " << app_name_ << " run --all-files\n";
    std::cout << "  " << app_name_ << " run src/main.cpp\n";
    std::cout << "  " << app_name_
              << " generate-gha > "
                 ".github/workflows/sniffercommit.yml\n\n";

    print_section_title("Subcommands");

    std::cout << "  init\n";

    std::cout << "      Create:\n";

    std::cout << "        - .sniffercommit.toml\n";

    std::cout << "        - .clang-format\n\n";

    std::cout << "      Options:\n";

    std::cout << "        --style "
                 "<google|llvm|chromium|mozilla|webkit|microsoft|gnu>\n";

    std::cout << "        --name <project-name>\n\n";

    std::cout << "  install\n";

    std::cout << "      Generate and install:\n";

    std::cout << "        .git/hooks/pre-commit\n\n";

    std::cout << "  run\n";

    std::cout << "      Execute configured checks.\n\n";

    std::cout << "      Modes:\n";

    std::cout << "        --all-files\n";

    std::cout << "        --staged\n";

    std::cout << "        <explicit files>\n\n";

    std::cout << "  generate-gha\n";

    std::cout << "      Generate production-grade "
                 "GitHub Actions workflow.\n\n";

    print_section_title("Global Options");

    for (const auto& opt : options_) {
      std::string left;

      if (!opt.short_flag.empty()) {
        left += std::string(opt.short_flag);
        left += ", ";
      }

      left += std::string(opt.long_flag);

      if (opt.has_value) left += " <value>";

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

 private:
  struct Subcommand {
    std::string name;
    std::string description;
  };

  std::string_view app_name_;
  std::string_view description_;
  std::string version_;
  std::span<char* const> args_;
  std::vector<Option> options_;
  std::vector<std::function<void(const std::string&)>> option_stores_;
  std::vector<bool*> flag_stores_;
  std::vector<Subcommand> subcommands_;
  std::string active_subcommand_;

  static constexpr size_t k_padding = 32;
};

}  // namespace sniffercommit

#endif  // !SNIFFERCOMMIT_ARGPARSER_HPP
