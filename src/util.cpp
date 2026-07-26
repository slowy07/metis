#include "sniffercommit/util.hpp"

#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <string>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

namespace sniffercommit::util {

bool command_exists(const std::string& cmd) {
#ifdef _WIN32
  std::string test = "where " + shell_escape(cmd) + " >nul 2>&1";
  return std::system(test.c_str()) == 0;
#else
  if (cmd.empty()) {
    return false;
  }
  if (cmd.find('/') != std::string::npos) {
    return ::access(cmd.c_str(), X_OK) == 0;
  }
  const char* path_env = std::getenv("PATH");
  if (path_env == nullptr) {
    return false;
  }
  std::string path_copy = path_env;
  size_t start = 0;
  while (start < path_copy.size()) {
    size_t end = path_copy.find(':', start);
    std::string dir = path_copy.substr(start, end - start);
    if (!dir.empty()) {
      std::string full = dir;
      full += '/';
      full += cmd;
      if (::access(full.c_str(), X_OK) == 0) {
        return true;
      }
    }
    if (end == std::string::npos) {
      break;
    }
    start = end + 1;
  }
  return false;
#endif
}

std::string shell_escape(const std::string& value) {
  std::string escaped = "'";

  for (char chr : value) {
    if (chr == '\'') {
      escaped += "'\\''";
    } else {
      escaped += chr;
    }
  }

  escaped += "'";
  return escaped;
}

CwdGuard::CwdGuard(const std::filesystem::path& target)
    : original_cwd(std::filesystem::current_path()) {
  std::filesystem::current_path(target);
}

CwdGuard::~CwdGuard() {
  try {
    std::filesystem::current_path(original_cwd);
  } catch (std::exception& error) {
    std::cerr << error.what() << "\n";
  }
}

}  // namespace sniffercommit::util
