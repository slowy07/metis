#include "sniffercommit/infrastructure/toolchain_factory.hpp"

#include <memory>

#include "sniffercommit/domain/ports/file_system.hpp"
#include "sniffercommit/domain/ports/shell_executor.hpp"
#include "sniffercommit/domain/ports/toolchain_provider.hpp"
#include "sniffercommit/infrastructure/linux_gcc_provider.hpp"
#include "sniffercommit/infrastructure/windows_gcc_provider.hpp"

namespace sniffercommit::infrastructure {

std::unique_ptr<domain::ports::IToolchainProvider> ToolchainFactory::create(
    const std::string& compiler, const std::string& version, domain::ports::IShellExecutor& shell,
    domain::ports::IFileSystem& fs) {
  if (compiler != "gcc") {
    return nullptr;
  }

#ifdef _WIN32
  return std::make_unique<WindowsGccProvider>(&shell, &fs, version);
#else
  (void)fs;
  return std::make_unique<LinuxGccProvider>(&shell, version);
#endif  // _WIN32
}
}  // namespace sniffercommit::infrastructure
