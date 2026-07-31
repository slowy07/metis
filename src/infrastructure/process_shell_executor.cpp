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

// RAII wrapper for FILE* from popen/_popen.
// Ensures pclose/_pclose is called even if exceptions are thrown.
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

// Executes a shell command and returns its stdout as a string.
//
// Platform differences:
//   Windows: uses _popen/_pclose (simpler, no fork)
//   Unix: uses fork + pipe + dup2 + execl + waitpid
//
// The Unix path is more complex because popen() doesn't give us access
// to the child PID for waitpid, and we need to handle signals correctly.
// fork() lets us redirect stdout/stderr to a pipe and wait for completion.
//
// Both paths strip the trailing newline from output.
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
  // Unix path: fork + pipe + exec
  // 1. Create a pipe for stdout communication
  // 2. Fork the process
  // 3. Child: redirect stdout to pipe, exec the command
  // 4. Parent: read from pipe, wait for child completion
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
    // Child process: close read end, redirect stdout to write end, exec
    ::close(fds[0]);
    ::dup2(fds[1], STDOUT_FILENO);
    ::close(fds[1]);
    ::execl("/bin/sh", "sh", "-c", cmd.c_str(), nullptr);
    ::_exit(127);  // exec failed
  }

  // Parent process: close write end, read from pipe
  ::close(fds[1]);

  std::array<char, 4096> buffer{};
  ssize_t n;
  while ((n = ::read(fds[0], buffer.data(), buffer.size() - 1)) > 0) {
    buffer[static_cast<size_t>(n)] = '\0';
    result += buffer.data();
  }
  ::close(fds[0]);

  // Wait for child to finish and get exit status
  int status;
  ::waitpid(pid, &status, 0);
#endif

  if (!result.empty() && result.back() == '\n') {
    result.pop_back();
  }

  return result;
}

// Like exec(), but captures both stdout and stderr, plus the exit code.
// Used for running checks where we need to report tool output on failure.
//
// stderr is merged into stdout via 2>&1 (Windows) or dup2(STDERR_FILENO)
// (Unix) so we get all output in one string.
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
  // Unix path: same as exec(), but also redirects stderr to the pipe
  // so we capture both stdout and stderr in one string.
  int fds[2];
  if (::pipe(fds) == -1) {
    return {.exit_code_ = 1, .output_ = "pipe() failed"};
  }

  pid_t pid = ::fork();
  if (pid == -1) {
    ::close(fds[0]);
    ::close(fds[1]);
    return {.exit_code_ = 1, .output_ = "fork() failed"};
  }

  if (pid == 0) {
    // Child: redirect both stdout and stderr to the pipe
    ::close(fds[0]);
    ::dup2(fds[1], STDOUT_FILENO);
    ::dup2(fds[1], STDERR_FILENO);
    ::close(fds[1]);
    ::execl("/bin/sh", "sh", "-c", cmd.c_str(), nullptr);
    ::_exit(127);
  }

  // Parent: read all output, wait for child, extract exit code
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
  // Extract the actual exit code from the wait status.
  // WIFEXITED: normal termination, WEXITSTATUS: the exit code.
  // WIFSIGNALED: killed by signal, 128 + signal number is the convention.
  if (WIFEXITED(status)) {
    code = WEXITSTATUS(status);
  } else if (WIFSIGNALED(status)) {
    code = 128 + WTERMSIG(status);
  }

  return {.exit_code_ = code, .output_ = output};
#endif
}

// Checks if a command exists in PATH.
// lazy: duplicates util::command_exists() — one should be deleted.
// Uses `where` on Windows, `access(X_OK)` on Unix.
// If the command contains '/', checks it as an absolute path directly.
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
