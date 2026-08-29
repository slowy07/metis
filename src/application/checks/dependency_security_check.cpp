#include "metis/application/checks/dependency_security_check.hpp"

#include <fmt/format.h>

#include <filesystem>
#include <string>
#include <vector>

#include "metis/domain/ports/shell_executor.hpp"

namespace metis::application::checks {
DependencySecurityCheck::DependencySecurityCheck(const domain::config::Check& config)
  : domain::Check(config.name, config.description, config.enabled, config.patterns, config.command,
                  config.args, config.timeout, config.severity) {}

std::string DependencySecurityCheck::validate(const std::filesystem::path& repo_root) const {
  (void)repo_root;
  return "";
}

DependencySecurityCheck::Tool DependencySecurityCheck::detect_tool(
    domain::ports::IShellExecutor* shell) {
  if (shell->command_exists("osv-scanner")) {
    return Tool::OSV_SCANNER;
  }

  if (shell->command_exists("grype")) {
    return Tool::GRYPE;
  }

  return Tool::NONE;
}

std::string DependencySecurityCheck::run_cve_scan(domain::ports::IShellExecutor* shell,
                                                  bool verbose) {
  auto tool = detect_tool(shell);
  if (tool == Tool::NONE) {
    return "[WARN] No CVE scanner found (install osv-scanner or grype)\n";
  }

  std::string cmd;
  if (tool == Tool::OSV_SCANNER) {
    cmd = "osv-scanner -r . --format table";
  } else {
    cmd = "grype dir:.";
  }

  if (verbose) {
    cmd += " -v";
  }

  auto result = shell->exec_captured(cmd);

  if (result.exit_code_ != 0 && result.exit_code_ != 1) {
    return fmt::format("[ERROR] CVE scan failed (exit {}): {}\n", result.exit_code_,
                       result.output_);
  }

  return result.output_;
}

std::string DependencySecurityCheck::run_sbom_generation(domain::ports::IShellExecutor* shell,
                                                         bool verbose) {
  if (!shell->command_exists("syft")) {
    return "[WARN] syft not found: skip SBOM generation\n";
  }

  std::string cmd = "syft dir:. -o cyclonedx-json=sbom.json";
  if (verbose) {
    cmd += " -v";
  }

  auto result = shell->exec_captured(cmd);
  if (result.exit_code_ != 0) {
    return fmt::format("[ERROR] SBOM generation failed: {}\n", result.output_);
  }

  return "[INFO] SBOM written to sbom.json\n";
}

domain::CheckResult DependencySecurityCheck::execute(const std::vector<std::string>& /*files*/,
                                                     domain::ports::IShellExecutor* shell,
                                                     bool verbose, bool dry_run) {
  if (dry_run) {
    return {.exit_code = 0, .output = {}};
  }

  std::string output;
  if (verbose) {
    output += "[metis] [DEP-SECURITY] Running dependency security checks\n";
  }

  output += run_cve_scan(shell, verbose);
  output += run_sbom_generation(shell, verbose);

  bool has_cves =
      output.find("Vulnerability") != std::string::npos || output.find("CVE-") != std::string::npos;

  return {.exit_code = has_cves ? 1 : 0, .output = output};
}

}  // namespace metis::application::checks
