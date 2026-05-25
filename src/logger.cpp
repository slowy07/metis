#include "sniffercommit/logger.hpp"

#include <chrono>
#include <iomanip>
#include <string_view>

namespace sniffercommit {
void Logger::log(LogLevel level, std::string_view prefix, std::string_view msg) {
  if (level < level_) {
    return;
  }

  auto& stream = (level >= LogLevel::WARN) ? *err_ : *out_;

  auto now = std::chrono::system_clock::now();
  auto time = std::chrono::system_clock::to_time_t(now);
  stream << std::put_time(std::localtime(&time), "%H:%M:%S");

  switch (level) {
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
