#include "sniffercommit/logger.hpp"

#include <chrono>
#include <iomanip>
#include <string_view>

namespace sniffercommit {

static std::string_view prefix_for_level(LogLevel lvl) {
  switch (lvl) {
    case LogLevel::DEBUG: return "[DEBUG]";
    case LogLevel::INFO: return "[INFO]";
    case LogLevel::WARN: return "[WARN]";
    case LogLevel::ERROR: return "[ERROR]";
  }
  return "[UNKNOWN]";
}

void Logger::log(LogLevel lvl, std::string_view msg) {
  if (lvl < this->level) {
    return;
  }

  auto& stream = (lvl >= LogLevel::WARN) ? *err : *out;

  auto now = std::chrono::system_clock::now();
  auto time = std::chrono::system_clock::to_time_t(now);
  stream << std::put_time(std::localtime(&time), "%H:%M:%S");

  auto prefix = prefix_for_level(lvl);
  switch (lvl) {
    case LogLevel::DEBUG:
      stream << " \033[2m" << prefix << "\033[0m ";
      break;
    case LogLevel::INFO:
      stream << " \033[36m" << prefix << "\033[0m ";
      break;
    case LogLevel::WARN:
      stream << " \033[33m" << prefix << "\033[0m ";
      break;
    case LogLevel::ERROR:
      stream << " \033[31m" << prefix << "\033[0m ";
      break;
  }

  stream << msg << "\n";
}
}  // namespace sniffercommit
