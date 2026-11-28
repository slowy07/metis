#ifndef METIS_APPLICATION_SWITCH_TOOLCHAIN_USE_CASE_HPP
#define METIS_APPLICATION_SWITCH_TOOLCHAIN_USE_CASE_HPP

#include <string_view>

#include "metis/application/list_compilers_use_case.hpp"
#include "metis/domain/compiler/ports.hpp"

namespace metis::application {
class SwitchToolchainUseCase {
 public:
  explicit SwitchToolchainUseCase(const ListCompilersUseCase& list_use_case);
  void execute(std::string_view id, domain::compiler::ToolchainConfigRepository& repo) const;

 private:
  const ListCompilersUseCase& list_use_case_;
};
}  // namespace metis::application

#endif  // !METIS_APPLICATION_SWITCH_TOOLCHAIN_USE_CASE_HPP
