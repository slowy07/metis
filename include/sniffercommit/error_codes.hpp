#ifndef SNIFFERCOMMIT_ERROR_CODES_HPP
#define SNIFFERCOMMIT_ERROR_CODES_HPP

#include <cstdint>
#include <string_view>

namespace sniffercommit {

enum class ExitCode : std::uint8_t {
  SUCCESS = 0,
  GENERAL_ERROR = 1,
  INVALID_ARGUMENTS = 2,
  CONFIG_ERROR = 3,
  CHECK_FAILURE = 4,
  FORMAT_FAILURE = 5,
  MISSING_DEPENDENCY = 6,
  NOT_A_GIT_REPO = 7,
  FILESYSTEM_ERROR = 8,
  HOOK_INSTALL_ERROR = 9,
  WORKFLOW_GENERATION_ERROR = 10,
};

[[nodiscard]] constexpr std::string_view exit_code_name(ExitCode code) noexcept {
  switch (code) {
    case ExitCode::SUCCESS:
      return "SUCCESS";
    case ExitCode::GENERAL_ERROR:
      return "GENERAL_ERROR";
    case ExitCode::INVALID_ARGUMENTS:
      return "INVALID_ARGUMENTS";
    case ExitCode::CONFIG_ERROR:
      return "CONFIG_ERROR";
    case ExitCode::CHECK_FAILURE:
      return "CHECK_FAILURE";
    case ExitCode::FORMAT_FAILURE:
      return "FORMAT_FAILURE";
    case ExitCode::MISSING_DEPENDENCY:
      return "MISSING_DEPENDENCY";
    case ExitCode::NOT_A_GIT_REPO:
      return "NOT_A_GIT_REPO";
    case ExitCode::FILESYSTEM_ERROR:
      return "FILESYSTEM_ERROR";
    case ExitCode::HOOK_INSTALL_ERROR:
      return "HOOK_INSTALL_ERROR";
    case ExitCode::WORKFLOW_GENERATION_ERROR:
      return "WORKFLOW_GENERATION_ERROR";
  }

  return "UNKNOWN";
}

[[nodiscard]] constexpr std::string_view exit_code_description(ExitCode code) noexcept {
  switch (code) {
    case ExitCode::SUCCESS:
      return "All operations completed successfully";
    case ExitCode::GENERAL_ERROR:
      return "An unspecified error occurred";
    case ExitCode::INVALID_ARGUMENTS:
      return "Invalid command-line arguments";
    case ExitCode::CONFIG_ERROR:
      return "Configuration file error (missing, invalid, or corrupt)";
    case ExitCode::CHECK_FAILURE:
      return "One or more checks failed";
    case ExitCode::FORMAT_FAILURE:
      return "Code formatting failed";
    case ExitCode::MISSING_DEPENDENCY:
      return "Required external tool not found in PATH";
    case ExitCode::NOT_A_GIT_REPO:
      return "Not inside a Git repository";
    case ExitCode::FILESYSTEM_ERROR:
      return "File system operation failed";
    case ExitCode::HOOK_INSTALL_ERROR:
      return "Failed to install pre-commit hook";
    case ExitCode::WORKFLOW_GENERATION_ERROR:
      return "Failed to generate CI workflow";
  }

  return "Unknown error code";
}

}  // namespace sniffercommit

#endif  // !SNIFFERCOMMIT_ERROR_CODES_HPP
