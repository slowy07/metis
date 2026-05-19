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
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
  ArgParser(std::string_view name, std::string_view desc) : app_name(name), description(desc) {}

  template <Parsable T>
  ArgParser& add_option(std::string_view short_flag, std::string_view long_flag,
                        std::string_view desc, T& storage, T default_val = {}) {
    std::string default_str;
    if constexpr (std::is_same_v<T, std::string>) {
      default_str = default_val;
    } else {
      default_str = std::to_string(default_val);
    }
    options.emplace_back(Option{.short_flag = short_flag,
                                .long_flag = long_flag,
                                .description = desc,
                                .default_value = std::move(default_str),
                                .has_value = true});
    option_stores.emplace_back([&storage](const std::string& val) {
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
    options.emplace_back(Option{.short_flag = short_flag,
                                .long_flag = long_flag,
                                .description = desc,
                                .default_value = "false",
                                .has_value = false});
    flag_stores.emplace_back(&storage);
    return *this;
  }

  ArgParser& add_subcommand(std::string_view name, std::string_view desc) {
    subcommands.emplace_back(
        Subcommand{.name = std::string(name), .description = std::string(desc)});
    return *this;
  }

  ArgParser& set_version(std::string_view ver) {
    version = ver;
    return *this;
  }

  // NOLINTNEXTLINE(readability-function-cognitive-complexity)
  bool parse(int argc, char** argv) {
    auto argc_sz = static_cast<size_t>(argc);
    if (argc_sz < 2) {
      show_help();
      return false;
    }
    args = std::span(argv, argc_sz);

    for (size_t i = 1; i < argc_sz; ++i) {
      std::string_view arg =
          args[i];  // NOLINT(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
      if (arg == "--help" || arg == "-h") {
        show_help();
        return false;
      }
      if (!version.empty() && (arg == "--version" || arg == "-v")) {
        std::cout << app_name << " " << version << "\n";
        return false;
      }
    }

    if (!subcommands.empty()) {
      std::string_view first_arg =
          args[1];  // NOLINT(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
      if (!first_arg.starts_with("-")) {
        auto found = std::ranges::find_if(
            subcommands, [first_arg](const auto& cmd) { return cmd.name == first_arg; });

        if (found != subcommands.end()) {
          active_subcommand = found->name;
          return true;
        }
        std::cerr << "[ERROR] Unknown subcommand: " << first_arg << "\n\n";
        show_help();
        return false;
      }
    }

    for (size_t i = 0; i < args.size(); ++i) {
      std::string_view arg =
          args[i];  // NOLINT(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
      if (!arg.starts_with('-')) {
        continue;
      }

      auto opt_it = std::ranges::find_if(options, [arg](const Option& opt) {
        return opt.short_flag == arg || opt.long_flag == arg;
      });

      if (opt_it == options.end()) {
        if (!active_subcommand.empty()) {
          continue;
        }
        std::cerr << "[ERROR] Unknown option: " << arg << "\n\n";
        show_help();
        return false;
      }

      if (opt_it->has_value) {
        if (i + 1 >= args.size()) {
          std::cerr << "[ERROR] Option " << arg << " requires value\n";
          return false;
        }
        std::string value = std::string(
            args[++i]);  // NOLINT(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
        auto opt_idx = static_cast<size_t>(std::distance(options.begin(), opt_it));
        if (opt_idx < option_stores.size()) {
          option_stores[opt_idx](
              value);  // NOLINT(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
        }
      } else {
        auto opt_idx = static_cast<size_t>(std::distance(options.begin(), opt_it));
        if (opt_idx < flag_stores.size() &&
            flag_stores[opt_idx] !=
                nullptr) {  // NOLINT(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
          *flag_stores[opt_idx] =
              true;  // NOLINT(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
        }
      }
    }

    return true;
  }

  [[nodiscard]] std::string_view get_subcommand() const { return active_subcommand; }

  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
  static void print_aligned(std::string_view left, std::string_view right) {
    std::cout << "  " << left;

    if (left.size() < k_padding) {
      std::cout << std::string(k_padding - left.size(), ' ');
    } else {
      std::cout << "\n";
      std::cout << std::string(k_padding + 2, ' ');
    }

    std::cout << right << "\n";
  }

  static void print_section_title(std::string_view title) { std::cout << title << ":\n"; }

  void show_help() const {
    std::cout << app_name << " - " << description << "\n\n";

    std::cout << "Usage:\n";

    std::cout << "  " << app_name << " [OPTIONS] <SUBCOMMAND> [ARGS]\n\n";

    print_section_title("Core Workflow");

    for (const auto& cmd : subcommands) {
      print_aligned(cmd.name, cmd.description);
    }

    std::cout << "\n";

    print_section_title("Examples");
    std::cout << "  " << app_name << " init\n";
    std::cout << "  " << app_name << " init --style llvm\n";
    std::cout << "  " << app_name << " init --name ultra-slowy\n";
    std::cout << "  " << app_name << " install\n";
    std::cout << "  " << app_name << " run --all-files\n";
    std::cout << "  " << app_name << " run src/main.cpp\n";
    std::cout << "  " << app_name
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

    for (const auto& opt : options) {
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

    if (!version.empty()) {
      print_aligned("-v, --version", "Show version");
    }

    print_aligned("-h, --help", "Show help message");
  }

 private:
  struct Subcommand {
    std::string name;
    std::string description;
  };

  std::string_view app_name;
  std::string_view description;
  std::string version;
  std::span<char*> args;
  std::vector<Option> options;
  std::vector<std::function<void(const std::string&)>> option_stores;
  std::vector<bool*> flag_stores;
  std::vector<Subcommand> subcommands;
  std::string active_subcommand;

  static constexpr size_t k_padding = 32;
};

}  // namespace sniffercommit

#endif  // !SNIFFERCOMMIT_ARGPARSER_HPP
