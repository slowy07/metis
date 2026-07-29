#ifndef SNIFFERCOMMIT_INFRASTRUCTURE_ZIP_ARCHIVE_EXTRACTOR_HPP
#define SNIFFERCOMMIT_INFRASTRUCTURE_ZIP_ARCHIVE_EXTRACTOR_HPP

#include <filesystem>

#include "sniffercommit/domain/ports/archive_extractor.hpp"
#include "sniffercommit/domain/ports/shell_executor.hpp"

namespace sniffercommit::infrastructure {
class ZipArchiveExtractor : public domain::ports::IArchiveExtractor {
 public:
  explicit ZipArchiveExtractor(domain::ports::IShellExecutor& shell);

  [[nodiscard]] domain::ports::ExtractionResult extract(
      const std::filesystem::path& archive_path, const std::filesystem::path& dest_dir) override;

 private:
  domain::ports::IShellExecutor& shell_;
};
}  // namespace sniffercommit::infrastructure

#endif  // !SNIFFERCOMMIT_INFRASTRUCTURE_ZIP_ARCHIVE_EXTRACTOR_HPP
