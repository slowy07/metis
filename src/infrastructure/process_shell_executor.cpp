#include "sniffercommit/infrastructure/process_shell_executor.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef _WIN32
#include <io.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace sniffercommit::infrastructure {

namespace {

struct PipeDeleter {
  void operator()(FILE* file_ptr) const noexcept {
    if (file_ptr != nullptr) {
#ifdef _WIN32
      (void)_pclose(file_ptr);
#else
      (void)pclose(file_ptr);
#endif
    }
  }
};

using PipePtr = std::unique_ptr<FILE, PipeDeleter>;

}  // namespace

std::string ProcessShellExecutor::exec(const std::string& cmd) {
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

domain::ports::CapturedResult ProcessShellExecutor::exec_captured(const std::string& cmd) {
  std::string output;
#ifdef _WIN32
  std::array<char, 4096> buffer{};
  FILE* pipe = _popen((cmd + " 2>&1").c_str(), "r");
  if (!pipe) {
    return {1, "popen() failed: " + cmd};
  }
  while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
    output += buffer.data();
  }
  int code = _pclose(pipe);
  return {code, output};
#else
  int fds[2];
  if (::pipe(fds) == -1) {
    return {.exit_code = 1, .output = "pipe() failed"};
  }

  pid_t pid = ::fork();
  if (pid == -1) {
    ::close(fds[0]);
    ::close(fds[1]);
    return {.exit_code = 1, .output = "fork() failed"};
  }

  if (pid == 0) {
    ::close(fds[0]);
    ::dup2(fds[1], STDOUT_FILENO);
    ::dup2(fds[1], STDERR_FILENO);
    ::close(fds[1]);
    ::execl("/bin/sh", "sh", "-c", cmd.c_str(), nullptr);
    ::_exit(127);
  }

  ::close(fds[1]);

  std::array<char, 4096> buffer{};
  ssize_t n;
  while ((n = ::read(fds[0], buffer.data(), buffer.size() - 1)) > 0) {
    buffer[static_cast<size_t>(n)] = '\0';
    output += buffer.data();
  }
  ::close(fds[0]);

  int status = 0;
  ::waitpid(pid, &status, 0);
  int code = 1;
  if (WIFEXITED(status)) {
    code = WEXITSTATUS(status);
  } else if (WIFSIGNALED(status)) {
    code = 128 + WTERMSIG(status);
  }

  return {.exit_code = code, .output = output};
#endif
}

bool ProcessShellExecutor::command_exists(const std::string& cmd) {
#ifdef _WIN32
  if (cmd.empty()) {
    return false;
  }
  std::string test = "where " + cmd + " >nul 2>&1";
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

}  // namespace sniffercommit::infrastructure
