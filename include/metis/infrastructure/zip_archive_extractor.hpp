#ifndef METIS_INFRASTRUCTURE_ZIP_ARCHIVE_EXTRACTOR_HPP
#define METIS_INFRASTRUCTURE_ZIP_ARCHIVE_EXTRACTOR_HPP

#include <filesystem>

#include "metis/domain/ports/archive_extractor.hpp"
#include "metis/domain/ports/shell_executor.hpp"

namespace metis::infrastructure {
class ZipArchiveExtractor : public domain::ports::IArchiveExtractor {
 public:
  explicit ZipArchiveExtractor(domain::ports::IShellExecutor* shell);

  [[nodiscard]] domain::ports::ExtractionResult extract(
      const std::filesystem::path& archive_path, const std::filesystem::path& dest_dir) override;

 private:
  domain::ports::IShellExecutor* shell_;
};
}  // namespace metis::infrastructure

#endif  // !METIS_INFRASTRUCTURE_ZIP_ARCHIVE_EXTRACTOR_HPP
