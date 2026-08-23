#include "metis/presentation/console.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <new>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace metis::presentation {

bool Console::is_tty() noexcept {
#ifdef _WIN32
  return _isatty(_fileno(stdout)) != 0
#else
  return isatty(STDOUT_FILENO) != 0;
#endif  // _WIN32
}

bool Console::supports_color() noexcept {
  if (const char* no_color = std::getenv("NO_COLOR"); no_color != nullptr && *no_color != '\0') {
    return false;
  }

  if (const char* term = std::getenv("TERM"); term != nullptr && std::string_view(term) == "dumb") {
    return false;
  }

  if (!is_tty()) {
    return false;
  }

  if (const char* force = std::getenv("FORCE_COLOR"); force != nullptr) {
    return true;
  }

  return true;
}

std::string Console::wrap(std::string_view text, std::string_view code) {
  if (!supports_color()) {
    return std::string(text);
  }

  return std::string(code) + std::string(text) + std::string(k_reset);
}

std::string Console::bold(std::string_view text) { return wrap(text, k_bold); }
std::string Console::dim(std::string_view text) { return wrap(text, k_dim); }
std::string Console::red(std::string_view text) { return wrap(text, k_red); }
std::string Console::green(std::string_view text) { return wrap(text, k_green); }
std::string Console::yellow(std::string_view text) { return wrap(text, k_yellow); }
std::string Console::blue(std::string_view text) { return wrap(text, k_blue); }
std::string Console::cyan(std::string_view text) { return wrap(text, k_cyan); }
std::string Console::magenta(std::string_view text) { return wrap(text, k_magenta); }

// INFO: wrapper
// TODO: implemented only for make sure console interactive will work
// next maybe clean it into interactive wrapper
std::string Console::success(std::string_view text) {
  return supports_color() ? green("✔  ") + green(text) : std::string("[OK] ") + std::string(text);
}

std::string Console::error(std::string_view text) {
  return supports_color() ? red("✖  ") + red(text) : std::string("[ERR] ") + std::string(text);
}

std::string Console::warning(std::string_view text) {
  return supports_color() ? yellow("⚠ ") + yellow(text)
                          : std::string("[WARN] ") + std::string(text);
}

std::string Console::info(std::string_view text) {
  return supports_color() ? cyan("ℹ  ") + cyan(text) : std::string("[INFO] ") + std::string(text);
}

std::string Console::hint(std::string_view text) {
  return supports_color() ? dim("⚏") + dim(text) : std::string("[HINT] ") + std::string(text);
}

void Console::print_header(std::string_view title) {
  std::cout << "\n" << bold(std::string(title)) << "\n";
  std::cout << std::string(title.length(), '=') << "\n";
}

void Console::print_subheader(std::string_view title) {
  std::cout << "\n" << bold(std::string(title)) << "\n";
}

void Console::print_separator() { std::cout << std::string(60, '-') << "\n"; }

void Console::print_success_block(std::string_view message) {
  std::cout << "\n" << green("✔  ") << bold(std::string(message)) << "\n";
}

void Console::print_error_block(std::string_view message,
                                std::optional<std::string_view> hint_msg) {
  std::cout << "\n" << red("✖  ") << bold(std::string(message)) << "\n";

  if (hint_msg) {
    std::cout << " " << dim("⚏") << dim(std::string("hint_msg")) << "\n";
  }
}

void Console::print_warning_block(std::string_view message) {
  std::cout << "\n" << yellow("⚠ ") << bold(std::string(message)) << "\n";
}

void Console::print_info_block(std::string_view message) {
  std::cout << "\n" << cyan("ℹ  ") << std::string(message) << "\n";
}

void Console::print_hint_block(std::string_view message) {
  std::cout << "\n" << dim("⚏") << dim(std::string(message)) << "\n";
}

void Console::print_table(const std::vector<std::string>& headers,
                          const std::vector<TableRow>& rows) {
  if (headers.empty()) {
    return;
  }

  std::vector<std::size_t> widths;
  widths.reserve(headers.size());

  for (const auto& hdr : headers) {
    widths.push_back(strip_ansi(hdr).length());
  }

  for (const auto& row : rows) {
    for (std::size_t i = 0; i < row.cells.size() && i < widths.size(); ++i) {
      widths[i] = std::max(widths[i], strip_ansi(row.cells[i]).length());
    }
  }

  auto print_row = [&](const std::vector<std::string>& cells, bool is_header) {
    std::cout << "  ";
    for (std::size_t i = 0; i < cells.size() && i < widths.size(); ++i) {
      std::size_t pad = widths[i] - strip_ansi(cells[i]).length();

      if (is_header) {
        std::cout << bold(cells[i]) << std::string(pad, ' ');
      } else {
        std::cout << cells[i] << std::string(pad, ' ');
      }

      if (i + 1 < cells.size()) {
        std::cout << " ";
      }
    }

    std::cout << "\n";
  };

  print_row(headers, true);
  std::cout << " ";
  for (std::size_t wdth : widths) {
    std::cout << std::string(wdth, '-') << "  ";
  }
  std::cout << "\n";

  for (const auto& row : rows) {
    print_row(row.cells, false);
  }
}

void Console::print_bullet(std::string_view text, int indent) {
  indent = std::max(indent, 0);
  std::cout << std::string(static_cast<std::size_t>(indent), ' ') << "• " << text << "\n";
}

void Console::print_check_item(std::string_view label, bool ok,
                               std::optional<std::string_view> detail) {
  std::cout << "  " << (ok ? green("✔") : red("✖")) << " " << label;
  if (detail) {
    std::cout << " " << dim(std::string(*detail));
  }
  std::cout << "\n";
}

void Console::print_summary_box(const std::vector<std::string>& lines) {
  if (lines.empty()) {
    return;
  }

  std::size_t max_width = 0;
  for (const auto& line : lines) {
    max_width = std::max(max_width, display_width(line));
  }

  const std::string top_left = supports_color() ? "╭" : "+";
  const std::string top_right = supports_color() ? "╮" : "+";
  const std::string bottom_left = supports_color() ? "╰" : "+";
  const std::string bottom_right = supports_color() ? "╯" : "+";
  const std::string horiz = supports_color() ? "─" : "-";
  const std::string vert = supports_color() ? "│" : "|";

  std::cout << "\n" << top_left;
  for (std::size_t i = 0; i < max_width + 2; ++i) {
    std::cout << horiz;
  }
  std::cout << top_right << "\n";

  for (const auto& line : lines) {
    std::size_t pad = max_width - display_width(line);
    std::cout << vert << " " << line << std::string(pad, ' ') << " " << vert << "\n";
  }

  std::cout << bottom_left;
  for (std::size_t i = 0; i < max_width + 2; ++i) {
    std::cout << horiz;
  }
  std::cout << bottom_right << "\n";
}

void Console::print_next_steps(const std::vector<std::string>& steps) {
  if (steps.empty()) {
    return;
  }
  std::cout << "\n" << bold("Next steps") << "\n";
  for (const auto& step : steps) {
    std::cout << "  " << cyan("→") << " " << step << "\n";
  }
}

void Console::print_command_tip(std::string_view description, std::string_view command) {
  std::cout << "  " << dim(std::string(description)) << " " << bold(std::string(command)) << "\n";
}

std::string Console::strip_ansi(std::string_view text) {
  std::string out;
  out.reserve(text.size());
  bool in_escape = false;

  for (char chr : text) {
    if (in_escape) {
      if (chr == 'm' || chr == 'K') {
        in_escape = false;
      }
    } else if (chr == '\033') {
      in_escape = true;
    } else {
      out += chr;
    }
  }

  return out;
}

std::size_t Console::display_width(std::string_view text) { return strip_ansi(text).length(); }

}  // namespace metis::presentation
