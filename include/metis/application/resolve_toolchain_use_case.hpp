#ifndef METIS_APPLICATION_RESOLVE_TOOLCHAIN_USE_CASE_HPP
#define METIS_APPLICATION_RESOLVE_TOOLCHAIN_USE_CASE_HPP

#include <filesystem>
#include <optional>
#include <string_view>

#include "metis/application/list_compilers_use_case.hpp"
#include "metis/domain/compiler/ports.hpp"

namespace metis::application {
class ResolveToolchainUseCase {
 public:
  ResolveToolchainUseCase(const ListCompilersUseCase& list_use_case,
                          const domain::compiler::ToolchainConfigRepository& project,
                          const domain::compiler::ToolchainConfigRepository& global);

  [[nodiscard]] std::optional<std::filesystem::path> which(std::string_view command) const;

 private:
  const ListCompilersUseCase& list_use_case_;
  const domain::compiler::ToolchainConfigRepository& project_;
  const domain::compiler::ToolchainConfigRepository& global_;
};
}  // namespace metis::application

#endif  // !METIS_APPLICATION_RESOLVE_TOOLCHAIN_USE_CASE_HPP
