#ifndef SNIFFERCOMMIT_LOGGER_HPP
#define SNIFFERCOMMIT_LOGGER_HPP

#include <fmt/base.h>
#include <fmt/format.h>

#include <cstdint>
#include <fstream>
#include <iostream>
#include <string_view>

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

  void set_level(LogLevel level) { level_ = level; }
  void set_output(std::ostream& out) { out_ = &out; }
  void set_error_output(std::ostream& err) { err_ = &err; }

  void debug(std::string_view msg) { log(LogLevel::DEBUG, "[DEBUG]", msg); }
  void info(std::string_view msg) { log(LogLevel::INFO, "[INFO]", msg); }
  void warn(std::string_view msg) { log(LogLevel::WARN, "[WARN]", msg); }
  void error(std::string_view msg) { log(LogLevel::ERROR, "[ERROR]", msg); }

  template <typename... Args>
  void info_fmt(std::string_view fmt_str, Args&&... args) {
    info(fmt_str);
  }

 private:
  Logger() = default;

  void log(LogLevel leve, std::string_view prefix, std::string_view msg);

  LogLevel level_ = LogLevel::INFO;
  std::ostream* out_ = &std::cout;
  std::ostream* err_ = &std::cerr;
};

inline void log_debug(std::string_view msg) { Logger::instance().debug(msg); }
inline void log_info(std::string_view msg) { Logger::instance().info(msg); }
inline void log_warn(std::string_view msg) { Logger::instance().warn(msg); }
inline void log_error(std::string_view msg) { Logger::instance().error(msg); }

}  // namespace sniffercommit

#endif  // !SNIFFERCOMMIT_LOGGER_HPP
