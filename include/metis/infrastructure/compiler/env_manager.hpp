#ifndef METIS_INFRASTRUCTURE_COMPILER_ENV_MANAGER_HPP
#define METIS_INFRASTRUCTURE_COMPILER_ENV_MANAGER_HPP

#include <string_view>
#include <vector>

#include "metis/domain/compiler/ports.hpp"

namespace metis::infrastructure::compiler {

class PosixEnvManager final : public domain::compiler::EnvironmentManager {
 public:
  [[nodiscard]] std::vector<domain::compiler::EnvVar> env_for(
      const domain::compiler::Compiler& compiler) const override;
};

[[nodiscard]] std::string shell_quote(std::string_view value);
[[nodiscard]] std::string render_posix_env(const std::vector<domain::compiler::EnvVar>& vars);
}  // namespace metis::infrastructure::compiler

#endif  // !METIS_INFRASTRUCTURE_COMPILER_ENV_MANAGER_HPP
