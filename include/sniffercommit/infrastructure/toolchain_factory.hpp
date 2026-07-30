#ifndef SNIFFERCOMMIT_INFRASTRUCTURE_TOOLCHAIN_FACTORY_HPP
#define SNIFFERCOMMIT_INFRASTRUCTURE_TOOLCHAIN_FACTORY_HPP

#include <memory>
#include <string>

#include "sniffercommit/domain/ports/file_system.hpp"
#include "sniffercommit/domain/ports/shell_executor.hpp"
#include "sniffercommit/domain/ports/toolchain_provider.hpp"

namespace sniffercommit::infrastructure {

class ToolchainFactory {
 public:
  [[nodiscard]] static std::unique_ptr<domain::ports::IToolchainProvider> create(
      const std::string& compiler, const std::string& version, domain::ports::IShellExecutor* shell,
      domain::ports::IFileSystem* fs);
};
}  // namespace sniffercommit::infrastructure

#endif  // !SNIFFERCOMMIT_INFRASTRUCTURE_TOOLCHAIN_FACTORY_HPP
