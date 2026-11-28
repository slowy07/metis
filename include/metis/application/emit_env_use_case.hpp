#ifndef METIS_APPLICATION_EMIT_ENV_USE_CASE_HPP
#define METIS_APPLICATION_EMIT_ENV_USE_CASE_HPP

#include "metis/application/list_compilers_use_case.hpp"
#include "metis/domain/compiler/ports.hpp"

namespace metis::application {
class EmitEnvUseCase {
 public:
  EmitEnvUseCase(const ListCompilersUseCase& list_use_case,
                 const domain::compiler::ToolchainConfigRepository& project,
                 const domain::compiler::ToolchainConfigRepository& global,
                 const domain::compiler::EnvironmentManager& env_manager);
  [[nodiscard]] std::vector<domain::compiler::EnvVar> execute(
      std::optional<std::string_view> id) const;

 private:
  const ListCompilersUseCase& list_use_case_;
  const domain::compiler::ToolchainConfigRepository& project_;
  const domain::compiler::ToolchainConfigRepository& global_;
  const domain::compiler::EnvironmentManager& env_manager_;
};
}  // namespace metis::application

#endif  // !METIS_APPLICATION_EMIT_ENV_USE_CASE_HPP
