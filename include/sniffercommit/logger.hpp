#ifndef SNIFFERCOMMIT_LOGGER_HPP
#define SNIFFERCOMMIT_LOGGER_HPP

#include <fmt/format.h>

#include <cstdint>
#include <fstream>
#include <iostream>
#include <string_view>
#include <utility>

namespace sniffercommit {

enum class LogLevel : std::uint8_t {
  DEBUG = 0,
  INFO = 1,
  WARN = 2,
  ERROR = 3,
};

class Logger {
 public:
  static Logger& instance() {
    static Logger logger;
    return logger;
  }

  void set_level(LogLevel lvl) { level = lvl; }
  void set_output(std::ostream& output_stream) { out = &output_stream; }
  void set_error_output(std::ostream& error_stream) { err = &error_stream; }

  void debug(std::string_view msg) { log(LogLevel::DEBUG, msg); }
  void info(std::string_view msg) { log(LogLevel::INFO, msg); }
  void warn(std::string_view msg) { log(LogLevel::WARN, msg); }
  void error(std::string_view msg) { log(LogLevel::ERROR, msg); }

  template <typename... Args>
  void info_fmt(std::string_view fmt_str, Args&&... args) {
    info(fmt::vformat(fmt_str, fmt::make_format_args(std::forward<Args>(args)...)));
  }

 private:
  Logger() = default;

  void log(LogLevel lvl, std::string_view msg);

  LogLevel level = LogLevel::INFO;
  std::ostream* out = &std::cout;
  std::ostream* err = &std::cerr;
};

inline void log_debug(std::string_view msg) { Logger::instance().debug(msg); }
inline void log_info(std::string_view msg) { Logger::instance().info(msg); }
inline void log_warn(std::string_view msg) { Logger::instance().warn(msg); }
inline void log_error(std::string_view msg) { Logger::instance().error(msg); }

}  // namespace sniffercommit

#endif  // !SNIFFERCOMMIT_LOGGER_HPP
