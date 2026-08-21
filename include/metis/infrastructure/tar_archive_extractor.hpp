#ifndef METIS_INFRASTRUCTURE_TAR_ARCHIVE_EXTRACTOR_HPP
#define METIS_INFRASTRUCTURE_TAR_ARCHIVE_EXTRACTOR_HPP

#include <filesystem>

#include "metis/domain/ports/archive_extractor.hpp"
#include "metis/domain/ports/shell_executor.hpp"

namespace metis::infrastructure {
class TarArchiveExtractor : public domain::ports::IArchiveExtractor {
 public:
  explicit TarArchiveExtractor(domain::ports::IShellExecutor* shell);

  [[nodiscard]] domain::ports::ExtractionResult extract(
      const std::filesystem::path& archive_path, const std::filesystem::path& dest_dir) override;

 private:
  domain::ports::IShellExecutor* shell_;

  [[nodiscard]] static std::string tar_flags_for_extension(const std::filesystem::path& path);
};
}  // namespace metis::infrastructure

#endif  // !METIS_INFRASTRUCTURE_TAR_ARCHIVE_EXTRACTOR_HPP
