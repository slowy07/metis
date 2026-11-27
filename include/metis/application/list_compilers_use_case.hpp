#ifndef METIS_APPLICATION_LIST_COMPILERS_USE_CASE_HPP
#define METIS_APPLICATION_LIST_COMPILERS_USE_CASE_HPP

#include "metis/domain/compiler/ports.hpp"

namespace metis::application {
class ListCompilersUseCase {
 public:
  ListCompilersUseCase(const domain::compiler::CompilerDetector& detector,
                       const domain::compiler::CompilerRegistry& registry);
  [[nodiscard]] std::vector<domain::compiler::Compiler> execute(bool installed_only) const;

 private:
  const domain::compiler::CompilerDetector& detector_;
  const domain::compiler::CompilerRegistry& registry_;
};
}  // namespace metis::application

#endif  // !METIS_APPLICATION_LIST_COMPILERS_USE_CASE_HPP
