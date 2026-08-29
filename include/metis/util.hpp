#ifndef METIS_UTIL_HPP
#define METIS_UTIL_HPP

#include <filesystem>
#include <string>
#include <vector>

namespace metis::util {

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

}  // namespace metis::util

#endif  // !METIS_UTIL_HPP
