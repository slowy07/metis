#ifndef METIS_APPLICATION_CHECKS_DEPENDENCY_SECURITY_CHECK_HPP
#define METIS_APPLICATION_CHECKS_DEPENDENCY_SECURITY_CHECK_HPP

#include <cstdint>
#include <string>
#include <vector>

#include "metis/domain/check.hpp"
#include "metis/domain/config.hpp"

namespace metis::application::checks {
class DependencySecurityCheck : public domain::Check {
 public:
  explicit DependencySecurityCheck(const domain::config::Check& config);

  [[nodiscard]] std::string validate(const std::filesystem::path& repo_root) const override;
  [[nodiscard]] domain::CheckResult execute(const std::vector<std::string>& files,
                                            domain::ports::IShellExecutor* shell, bool verbose,
                                            bool dry_run) override;

 private:
  enum class Tool : std::uint8_t {
    OSV_SCANNER,
    GRYPE,
    NONE,
  };
  [[nodiscard]] static Tool detect_tool(domain::ports::IShellExecutor* shell);
  [[nodiscard]] static std::string run_cve_scan(domain::ports::IShellExecutor* shell, bool verbose);
  [[nodiscard]] static std::string run_sbom_generation(domain::ports::IShellExecutor* shell,
                                                       bool verbose);
};
}  // namespace metis::application::checks

#endif  // !METIS_APPLICATION_CHECKS_DEPENDENCY_SECURITY_CHECK_HPP
