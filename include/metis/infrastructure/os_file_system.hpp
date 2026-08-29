#ifndef METIS_INFRASTRUCTURE_OS_FILE_SYSTEM_HPP
#define METIS_INFRASTRUCTURE_OS_FILE_SYSTEM_HPP

#include "metis/domain/ports/file_system.hpp"

namespace metis::infrastructure {

class OsFileSystem : public domain::ports::IFileSystem {
 public:
  [[nodiscard]] bool exists(const std::filesystem::path& path) override;
  [[nodiscard]] bool create_directories(const std::filesystem::path& path) override;
  [[nodiscard]] bool write_file(const std::filesystem::path& path,
                                const std::string& content) override;
  [[nodiscard]] std::string read_file(const std::filesystem::path& path) override;
  [[nodiscard]] bool set_permissions(const std::filesystem::path& path,
                                     std::filesystem::perms perms,
                                     std::filesystem::perm_options opts) override;
  [[nodiscard]] std::filesystem::path current_path() override;
  [[nodiscard]] std::filesystem::path absolute(const std::filesystem::path& p) override;
};

}  // namespace metis::infrastructure

#endif
