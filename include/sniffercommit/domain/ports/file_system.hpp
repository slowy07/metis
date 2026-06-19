#ifndef SNIFFERCOMMIT_DOMAIN_PORTS_FILE_SYSTEM_HPP
#define SNIFFERCOMMIT_DOMAIN_PORTS_FILE_SYSTEM_HPP

#include <filesystem>
#include <string>

namespace sniffercommit::domain::ports {

struct IFileSystem {
  virtual ~IFileSystem() = default;

  [[nodiscard]] virtual bool exists(const std::filesystem::path& path) = 0;
  [[nodiscard]] virtual bool create_directories(const std::filesystem::path& path) = 0;
  [[nodiscard]] virtual bool write_file(const std::filesystem::path& path,
                                        const std::string& content) = 0;
  [[nodiscard]] virtual std::string read_file(const std::filesystem::path& path) = 0;
  [[nodiscard]] virtual bool remove(const std::filesystem::path& path) = 0;
  [[nodiscard]] virtual bool set_permissions(const std::filesystem::path& path,
                                             std::filesystem::perms perms,
                                             std::filesystem::perm_options opts) = 0;
  [[nodiscard]] virtual std::filesystem::path current_path() = 0;
  [[nodiscard]] virtual std::filesystem::path absolute(const std::filesystem::path& p) = 0;
};

}  // namespace sniffercommit::domain::ports

#endif
