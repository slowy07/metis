#ifndef METIS_PRESENTATION_CONSOLE_HPP
#define METIS_PRESENTATION_CONSOLE_HPP

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace metis::presentation {
class Console {
 public:
  Console() = delete;

  [[nodiscard]] static bool supports_color() noexcept;
  [[nodiscard]] static bool is_tty() noexcept;
  [[nodiscard]] static std::string bold(std::string_view text);
  [[nodiscard]] static std::string dim(std::string_view text);
  [[nodiscard]] static std::string red(std::string_view text);
  [[nodiscard]] static std::string green(std::string_view text);
  [[nodiscard]] static std::string yellow(std::string_view text);
  [[nodiscard]] static std::string blue(std::string_view text);
  [[nodiscard]] static std::string cyan(std::string_view text);
  [[nodiscard]] static std::string magenta(std::string_view text);
  [[nodiscard]] static std::string success(std::string_view text);
  [[nodiscard]] static std::string error(std::string_view text);
  [[nodiscard]] static std::string warning(std::string_view text);
  [[nodiscard]] static std::string info(std::string_view text);
  [[nodiscard]] static std::string hint(std::string_view text);
  static void print_header(std::string_view title);
  static void print_subheader(std::string_view title);
  static void print_separator();

  static void print_success_block(std::string_view message);
  static void print_error_block(std::string_view message,
                                std::optional<std::string_view> hint = std::nullopt);
  static void print_warning_block(std::string_view message);
  static void print_info_block(std::string_view message);
  static void print_hint_block(std::string_view message);

  struct TableRow {
    std::vector<std::string> cells;
  };

  static void print_table(const std::vector<std::string>& headers,
                          const std::vector<TableRow>& rows);

  static void print_bullet(std::string_view text, int indent = 2);
  static void print_check_item(std::string_view label, bool ok,
                               std::optional<std::string_view> detail = std::nullopt);
  static void print_summary_box(const std::vector<std::string>& lines);
  static void print_next_steps(const std::vector<std::string>& steps);
  static void print_command_tip(std::string_view description, std::string_view command);

 private:
  [[nodiscard]] static std::string wrap(std::string_view text, std::string_view code);
  [[nodiscard]] static std::string strip_ansi(std::string_view text);
  [[nodiscard]] static std::size_t display_width(std::string_view text);

  static constexpr std::string_view k_reset = "\033[0m";
  static constexpr std::string_view k_bold = "\033[1m";
  static constexpr std::string_view k_dim = "\033[2m";
  static constexpr std::string_view k_red = "\033[31m";
  static constexpr std::string_view k_green = "\033[32m";
  static constexpr std::string_view k_yellow = "\033[33m";
  static constexpr std::string_view k_blue = "\033[34m";
  static constexpr std::string_view k_magenta = "\033[35m";
  static constexpr std::string_view k_cyan = "\033[36m";
};
}  // namespace metis::presentation

#endif  // !METIS_PRESENTATION_CONSOLE_HPP
