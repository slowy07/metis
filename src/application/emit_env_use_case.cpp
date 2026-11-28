#include "metis/application/emit_env_use_case.hpp"

#include <stdexcept>
#include <vector>

#include "metis/domain/compiler/ports.hpp"

namespace metis::application {
EmitEnvUseCase::EmitEnvUseCase(const ListCompilersUseCase& list_use_case,
                               const domain::compiler::ToolchainConfigRepository& project,
                               const domain::compiler::ToolchainConfigRepository& global,
                               const domain::compiler::EnvironmentManager& env_manager)
    : list_use_case_(list_use_case),
      project_(project),
      global_(global),
      env_manager_(env_manager) {}

std::vector<domain::compiler::EnvVar> EmitEnvUseCase::execute(
    std::optional<std::string_view> id) const {
  std::string target_id;
  if (id) {
    target_id = std::string(*id);
  } else {
    auto pinned = project_.default_toolchain();
    if (!pinned) {
      pinned = global_.default_toolchain();
    }
    if (!pinned) {
      throw std::runtime_error(
          "no toolchain pinned; run 'metis toolchain use <id>' or pass an "
          "explicit id");
    }
    target_id = *pinned;
  }

  for (const auto& c : list_use_case_.execute(/*installed_only=*/false)) {
    if (c.id == target_id) {
      return env_manager_.env_for(c);
    }
  }
  throw std::runtime_error("unknown toolchain '" + target_id +
                           "'; run 'metis toolchain list' to see available "
                           "toolchains");
}

}  // namespace metis::application
