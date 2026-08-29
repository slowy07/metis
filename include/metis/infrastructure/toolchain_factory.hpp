#ifndef METIS_INFRASTRUCTURE_TOOLCHAIN_FACTORY_HPP
#define METIS_INFRASTRUCTURE_TOOLCHAIN_FACTORY_HPP

#include <memory>
#include <string>

#include "metis/domain/ports/file_system.hpp"
#include "metis/domain/ports/shell_executor.hpp"
#include "metis/domain/ports/toolchain_provider.hpp"

namespace metis::infrastructure {

class ToolchainFactory {
 public:
  [[nodiscard]] static std::unique_ptr<domain::ports::IToolchainProvider> create(
      const std::string& compiler, const std::string& version, domain::ports::IShellExecutor* shell,
      domain::ports::IFileSystem* fs);
};
}  // namespace metis::infrastructure

#endif  // !METIS_INFRASTRUCTURE_TOOLCHAIN_FACTORY_HPP
