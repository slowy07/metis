#ifndef SNIFFERCOMMIT_DOMAIN_ERROR_CODES_HPP
#define SNIFFERCOMMIT_DOMAIN_ERROR_CODES_HPP

#include <cstdint>

namespace sniffercommit::domain {

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

}  // namespace sniffercommit::domain

#endif
