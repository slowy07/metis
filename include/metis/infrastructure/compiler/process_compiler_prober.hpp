#ifndef METIS_INFRASTRUCTURE_COMPILER_PROCESS_COMPILER_PROBER_HPP
#define METIS_INFRASTRUCTURE_COMPILER_PROCESS_COMPILER_PROBER_HPP

#include "metis/domain/compiler/ports.hpp"

namespace metis::infrastructure::compiler {
class ProcessCompilerProber final : public domain::compiler::CompilerProber {
 public:
  [[nodiscard]] domain::compiler::ProbedCompiler probe(
      const std::filesystem::path& cxx) const override;
};
}  // namespace metis::infrastructure::compiler

#endif  // !METIS_INFRASTRUCTURE_COMPILER_PROCESS_COMPILER_PROBER_HPP
