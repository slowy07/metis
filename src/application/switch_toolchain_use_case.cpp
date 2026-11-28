#include "metis/application/switch_toolchain_use_case.hpp"

#include <stdexcept>
#include <string>

#include "metis/application/list_compilers_use_case.hpp"

namespace metis::application {
SwitchToolchainUseCase::SwitchToolchainUseCase(const ListCompilersUseCase& list_use_case)
    : list_use_case_(list_use_case) {}

void SwitchToolchainUseCase::execute(std::string_view id,
                                     domain::compiler::ToolchainConfigRepository& repo) const {
  auto all = list_use_case_.execute(false);
  for (const auto& c : all) {
    if (c.id == id) {
      repo.set_default(id);
      return;
    }
  }

  throw std::runtime_error("unknown toolchain '" + std::string(id) +
                           "' run 'metis toolchain list' to see available toolchains");
}
}  // namespace metis::application
