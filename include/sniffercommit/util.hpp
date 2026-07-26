#ifndef SNIFFERCOMMIT_UTIL_HPP
#define SNIFFERCOMMIT_UTIL_HPP

#include <filesystem>
#include <string>
#include <vector>

namespace sniffercommit::util {

// ponytail: shell_escape and command_exists are the only non-dead members.
// exec_cmd, exec_captured, PipeDeleter, CapturedResult, matches_pattern, is_excluded
// were all dead code (duplicated in infrastructure/ or unused).

// NOTE: escape string for safe use in single-quoted shell context
[[nodiscard]] std::string shell_escape(const std::string& value);

// NOTE: check if command exists in PATH
[[nodiscard]] bool command_exists(const std::string& cmd);

class CwdGuard {
 public:
  explicit CwdGuard(const std::filesystem::path& target);
  ~CwdGuard();

  CwdGuard(const CwdGuard&) = delete;
  CwdGuard& operator=(const CwdGuard&) = delete;
  CwdGuard(CwdGuard&&) = delete;
  CwdGuard& operator=(CwdGuard&&) = delete;

 private:
  std::filesystem::path original_cwd;
};

}  // namespace sniffercommit::util

#endif  // !SNIFFERCOMMIT_UTIL_HPP
