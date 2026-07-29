#ifndef SNIFFERCOMMIT_INFRASTRUCTURE_TAR_ARCHIVE_EXTRACTOR_HPP
#define SNIFFERCOMMIT_INFRASTRUCTURE_TAR_ARCHIVE_EXTRACTOR_HPP

#include <filesystem>

#include "sniffercommit/domain/ports/archive_extractor.hpp"
#include "sniffercommit/domain/ports/shell_executor.hpp"

namespace sniffercommit::infrastructure {
class TarArchiveExtractor : public domain::ports::IArchiveExtractor {
 public:
  explicit TarArchiveExtractor(domain::ports::IShellExecutor& shell);

  [[nodiscard]] domain::ports::ExtractionResult extract(
      const std::filesystem::path& archive_path, const std::filesystem::path& dest_dir) override;

 private:
  domain::ports::IShellExecutor& shell_;

  [[nodiscard]] static std::string tar_flags_for_extension(const std::filesystem::path& path);
};
}  // namespace sniffercommit::infrastructure

#endif  // !SNIFFERCOMMIT_INFRASTRUCTURE_TAR_ARCHIVE_EXTRACTOR_HPP
