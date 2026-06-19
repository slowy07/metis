#ifndef SNIFFERCOMMIT_DOMAIN_PORTS_LOGGER_PORT_HPP
#define SNIFFERCOMMIT_DOMAIN_PORTS_LOGGER_PORT_HPP

#include <string_view>

namespace sniffercommit::domain::ports {

struct ILogger {
  virtual ~ILogger() = default;

  virtual void debug(std::string_view msg) = 0;
  virtual void info(std::string_view msg) = 0;
  virtual void warn(std::string_view msg) = 0;
  virtual void error(std::string_view msg) = 0;
};

}  // namespace sniffercommit::domain::ports

#endif
