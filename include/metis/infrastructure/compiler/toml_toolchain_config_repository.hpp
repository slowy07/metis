#ifndef METIS_INFRASTRUCTURE_COMPILER_TOML_TOOLCHAIN_CONFIG_REPOSITORY_HPP
#define METIS_INFRASTRUCTURE_COMPILER_TOML_TOOLCHAIN_CONFIG_REPOSITORY_HPP

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include "metis/domain/compiler/ports.hpp"

namespace metis::infrastructure::compiler {

class TomlToolchainConfigRepository final : public domain::compiler::ToolchainConfigRepository {
 public:
  explicit TomlToolchainConfigRepository(std::filesystem::path path);
  [[nodiscard]] std::filesystem::path path() const override;
  [[nodiscard]] std::optional<std::string> default_toolchain() const override;
  void set_default(std::string_view id) override;

 private:
  std::filesystem::path path_;
};

[[nodiscard]] std::filesystem::path find_project_config_path(std::filesystem::path start);
[[nodiscard]] std::filesystem::path find_global_config_path();

}  // namespace metis::infrastructure::compiler

#endif  // !METIS_INFRASTRUCTURE_COMPILER_TOML_TOOLCHAIN_CONFIG_REPOSITORY_HPP
