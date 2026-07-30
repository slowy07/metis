#include "sniffercommit/infrastructure/toolchain_factory.hpp"

#include <cctype>
#include <memory>

#include "sniffercommit/domain/ports/file_system.hpp"
#include "sniffercommit/domain/ports/shell_executor.hpp"
#include "sniffercommit/domain/ports/toolchain_provider.hpp"
#include "sniffercommit/infrastructure/linux_gcc_provider.hpp"

namespace sniffercommit::infrastructure {

std::unique_ptr<domain::ports::IToolchainProvider> ToolchainFactory::create(
    const std::string& compiler, const std::string& version, domain::ports::IShellExecutor* shell,
    [[maybe_unused]] domain::ports::IFileSystem* fs) {
  std::string comp = compiler;

  for (char& chr : comp) {
    chr = static_cast<char>(std::tolower(static_cast<unsigned char>(chr)));
  }

#ifdef _WIN32
  if (comp == "gcc") {
    return std::make_unique<WindowsGccProvider>(shell, fs, version);
  }
  if (comp == "clang") {
    return std::make_unique<WindowsClangProvider>(shell, fs, version);
  }
#elif defined(__APPLE__)
  if (comp == "clang") {
    return std::make_unique<MacosClangProvider>(shell, fs, version);
  }
  if (comp == "gcc") {
    return nullptr;
  }
#else   // Linux
  if (comp == "gcc" || comp == "clang") {
    return std::make_unique<LinuxGccProvider>(shell, comp, version);
  }
#endif  // _WIN32
  return nullptr;
}
}  // namespace sniffercommit::infrastructure
