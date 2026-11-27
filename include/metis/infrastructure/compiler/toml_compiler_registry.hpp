#ifndef METIS_INFRASTRUCTURE_COMPILER_TOML_COMPILER_REGISTRY_HPP
#define METIS_INFRASTRUCTURE_COMPILER_TOML_COMPILER_REGISTRY_HPP

#include <filesystem>
#include <vector>

#include "metis/domain/compiler/compiler.hpp"
#include "metis/domain/compiler/ports.hpp"

namespace metis::infrastructure::compiler {

class TomlCompilerRegistry final : public domain::compiler::CompilerRegistry {
 public:
  explicit TomlCompilerRegistry(std::filesystem::path registry_path);
  [[nodiscard]] std::optional<domain::compiler::Compiler> find(std::string_view id) const override;
  [[nodiscard]] std::vector<domain::compiler::Compiler> list() const override;
  void save(const domain::compiler::Compiler& compiler) override;
  void remove(std::string_view id) override;

 private:
  [[nodiscard]] std::vector<domain::compiler::Compiler> load() const;
  void write(const std::vector<domain::compiler::Compiler>& compilers) const;

  std::filesystem::path registry_path_;
};
}  // namespace metis::infrastructure::compiler

#endif  // !METIS_INFRASTRUCTURE_COMPILER_TOML_COMPILER_REGISTRY_HPP
