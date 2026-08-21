#ifndef METIS_ARGPARSER_HPP
#define METIS_ARGPARSER_HPP

#include <concepts>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace metis {

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
  ArgParser(std::string_view name, std::string_view desc);

  template <Parsable T>
  ArgParser& add_option(std::string_view short_flag, std::string_view long_flag,
                        std::string_view desc, T& storage, T default_val = {}) {
    std::string default_str;
    if constexpr (std::is_same_v<T, std::string>) {
      default_str = default_val;
    } else {
      default_str = std::to_string(default_val);
    }
    options_.emplace_back(Option{.short_flag = short_flag,
                                 .long_flag = long_flag,
                                 .description = desc,
                                 .default_value = std::move(default_str),
                                 .has_value = true});
    option_stores_.emplace_back([&storage](const std::string& val) {
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
                      std::string_view desc, bool& storage);

  ArgParser& add_subcommand(std::string_view name, std::string_view desc);

  ArgParser& set_version(std::string_view ver);

  bool parse(int argc, char** argv);

  [[nodiscard]] std::string_view get_subcommand() const;

  void show_help() const;

 private:
  struct Subcommand {
    std::string name;
    std::string description;
  };

  static void print_aligned(std::string_view left, std::string_view right);
  static void print_section_title(std::string_view title);

  std::string_view app_name_;
  std::string_view description_;
  std::string version_;
  std::span<char*> args_;
  std::vector<Option> options_;
  std::vector<std::function<void(const std::string&)>> option_stores_;
  std::vector<bool*> flag_stores_;
  std::vector<Subcommand> subcommands_;
  std::string active_subcommand_;

  static constexpr size_t k_padding = 32;
};

}  // namespace metis

#endif  // !METIS_ARGPARSER_HPP
