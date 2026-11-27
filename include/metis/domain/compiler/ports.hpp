#ifndef METIS_DOMAIN_COMPILER_PORTS_HPP
#define METIS_DOMAIN_COMPILER_PORTS_HPP

#include <optional>
#include <string_view>
#include <vector>

#include "metis/domain/compiler/compiler.hpp"

namespace metis::domain::compiler {
struct ProbedCompiler {
  CompilerVersion version;
  CompilerCapabilities capabilities;
};

class CompilerDetector {
 public:
  virtual ~CompilerDetector() = default;
  [[nodiscard]] virtual std::vector<Compiler> detect() const = 0;
};

class CompilerRegistry {
 public:
  virtual ~CompilerRegistry() = default;
  [[nodiscard]] virtual std::optional<Compiler> find(std::string_view id) const = 0;
  [[nodiscard]] virtual std::vector<Compiler> list() const = 0;
  virtual void save(const Compiler& compiler) = 0;
  virtual void remove(std::string_view id) = 0;
};

class CompilerProber {
 public:
  virtual ~CompilerProber() = default;
  [[nodiscard]] virtual ProbedCompiler probe(const std::filesystem::path& cxx) const = 0;
};

}  // namespace metis::domain::compiler

#endif  // !METIS_DOMAIN_COMPILER_PORTS_HPP
