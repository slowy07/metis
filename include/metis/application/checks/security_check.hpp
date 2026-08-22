#ifndef METIS_APPLICATION_CHECKS_SECURITY_CHECK_HPP
#define METIS_APPLICATION_CHECKS_SECURITY_CHECK_HPP

#include <filesystem>
#include <regex>
#include <string>
#include <string_view>
#include <vector>

#include "metis/domain/check.hpp"
#include "metis/domain/config.hpp"
#include "metis/domain/ports/shell_executor.hpp"

namespace metis::application::checks {
class SecurityCheck : public domain::Check {
 public:
  explicit SecurityCheck(const domain::config::Check& config);
  [[nodiscard]] domain::CheckResult execute(const std::vector<std::string>& files,
                                            domain::ports::IShellExecutor* shell, bool verbose,
                                            bool dry_run) override;

 private:
  struct Pattern {
    std::regex regex;
    std::string category;
    std::string description;
  };

  std::vector<Pattern> patterns_;
  void init_patterns();
  [[nodiscard]] static bool is_comment_line(std::string_view line);
  [[nodiscard]] std::string scan_file(const std::filesystem::path& file_path) const;
};
}  // namespace metis::application::checks

#endif  // !METIS_APPLICATION_CHECKS_SECURITY_CHECK_HPP
