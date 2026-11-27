#ifndef METIS_INFRASTRUCTURE_COMPILER_PATH_COMPILER_DETECTOR_HPP
#define METIS_INFRASTRUCTURE_COMPILER_PATH_COMPILER_DETECTOR_HPP

#include <vector>

#include "metis/domain/compiler/compiler.hpp"
#include "metis/domain/compiler/ports.hpp"

namespace metis::infrastructure::compiler {
class PathCompilerDetector final : public domain::compiler::CompilerDetector {
 public:
  explicit PathCompilerDetector(const domain::compiler::CompilerProber& prober);
  [[nodiscard]] std::vector<domain::compiler::Compiler> detect() const override;

 private:
  const domain::compiler::CompilerProber& prober_;
};
}  // namespace metis::infrastructure::compiler

#endif  // !METIS_INFRASTRUCTURE_COMPILER_PATH_COMPILER_DETECTOR_HPP
