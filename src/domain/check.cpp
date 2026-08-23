#include "metis/domain/check.hpp"

#include <string>
#include <vector>

#include "metis/util.hpp"

namespace metis::domain {

Check::Check(std::string name, std::string description, bool enabled,
             std::vector<std::string> file_patterns, std::string command,
             std::vector<std::string> arguments, int timeout, std::string severity)
  : name_(std::move(name))
  , description_(std::move(description))
  , enabled_(enabled)
  , file_patterns_(std::move(file_patterns))
  , command_(std::move(command))
  , arguments_(std::move(arguments))
  , timeout_(timeout)
  , severity_(std::move(severity)) {}

std::string Check::command_line(const std::vector<std::string>& files) const {
  std::string cmd = util::shell_escape(command_);
  for (const auto& arg : arguments_) {
    cmd += " ";
    cmd += util::shell_escape(arg);
  }
  for (const auto& file : files) {
    cmd += " ";
    cmd += util::shell_escape(file);
  }
  return cmd;
}

}  // namespace metis::domain
