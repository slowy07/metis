#ifndef SNIFFERCOMMIT_UTIL_HPP
#define SNIFFERCOMMIT_UTIL_HPP

#include <cstdio>
#include <filesystem>
#include <memory>
#include <vector>
#include <string>

namespace sniffercommit::util {
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

// NOTE: execute shell command and caputre its stdout
// using dynamic buffer path
// @throws std::runtime_error if popen() failed
[[nodiscard]] std::string exec_cmd(const std::string& cmd);

// NOTE: check if command exists in PATH
[[nodiscard]] bool command_exists(const std::string& cmd);

// NOTE: escape string for safe use in single-quoted shell context
[[nodiscard]] std::string shell_escape(const std::string& value);

class CwdGuard {
 public:
  explicit CwdGuard(const std::filesystem::path& target);
  ~CwdGuard();

  // NOTE: non copyable, moveable
  CwdGuard(const CwdGuard&) = delete;
  CwdGuard& operator=(const CwdGuard&) = delete;
  CwdGuard(CwdGuard&&) = delete;
  CwdGuard& operator=(CwdGuard&&) = delete;

 private:
  std::filesystem::path original_cwd_;
};

// NOTE: check if a file path matches any of the given glob-like patterns
[[nodiscard]] bool matches_pattern(const std::string& file, const std::vector<std::string>& patterns);

// NOTE:check if a file path matches any exclusion rule
[[nodiscard]] bool is_excluded(const std::string& file, const std::vector<std::string>& excludes);

}  // namespace sniffercommit::init

#endif  // !SNIFFERCOMMIT_UTIL_HPP
