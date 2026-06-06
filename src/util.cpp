#include "sniffercommit/util.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

#ifdef _WIN32
#include <io.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace sniffercommit::util {

std::string exec_cmd(const std::string& cmd) {
  std::string result;
  result.reserve(4096);

#ifdef _WIN32
  PipePtr pipe(_popen(cmd.c_str(), "r"));
  if (!pipe) {
    throw std::runtime_error("popen() failed " + cmd);
  }

  std::array<char, 4096> buffer{};
  while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe.get()) != nullptr) {
    result += buffer.data();
  }
#else
  int fds[2];
  if (::pipe(fds) == -1) {
    throw std::runtime_error("pipe() failed");
  }

  pid_t pid = ::fork();
  if (pid == -1) {
    ::close(fds[0]);
    ::close(fds[1]);
    throw std::runtime_error("fork() failed");
  }

  if (pid == 0) {
    ::close(fds[0]);
    ::dup2(fds[1], STDOUT_FILENO);
    ::close(fds[1]);
    ::execl("/bin/sh", "sh", "-c", cmd.c_str(), nullptr);
    ::_exit(127);
  }

  ::close(fds[1]);

  std::array<char, 4096> buffer{};
  ssize_t n;
  while ((n = ::read(fds[0], buffer.data(), buffer.size() - 1)) > 0) {
    buffer[static_cast<size_t>(n)] = '\0';
    result += buffer.data();
  }
  ::close(fds[0]);

  int status;
  ::waitpid(pid, &status, 0);
#endif

  if (!result.empty() && result.back() == '\n') {
    result.pop_back();
  }

  return result;
}

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

bool matches_pattern(const std::string& file, const std::vector<std::string>& patterns) {
  if (patterns.empty()) {
    return true;
  }

  return std::ranges::any_of(patterns, [&file](const auto& pattern) {
    if (pattern.empty()) {
      return true;
    }

    if (pattern.starts_with("*.") && file.ends_with(pattern.substr(1))) {
      return true;
    }

    if (pattern.ends_with("/**") && file.starts_with(pattern.substr(0, pattern.size() - 3) + "/")) {
      return true;
    }

    if (pattern.starts_with("**/")) {
      std::string suffix = pattern.substr(3);
      if (file.ends_with(suffix)) {
        return true;
      }
    }

    if (file == pattern || file.starts_with(pattern)) {
      return true;
    }

    return false;
  });
}

bool is_excluded(const std::string& file, const std::vector<std::string>& excludes) {
  for (const auto& excl : excludes) {
    if (file == excl) {
      return true;
    }

    if (excl.starts_with("*.") && file.ends_with(excl.substr(1))) {
      return true;
    }

    std::string norm_e = excl;

    if (!norm_e.empty() && norm_e.back() != '/') {
      norm_e += '/';
    }

    if (file.starts_with(norm_e)) {
      return true;
    }
  }

  return false;
}

}  // namespace sniffercommit::util
