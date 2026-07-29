#ifndef SNIFFERCOMMIT_DOMAIN_PORTS_ARCHIVE_EXTRACTOR_HPP
#define SNIFFERCOMMIT_DOMAIN_PORTS_ARCHIVE_EXTRACTOR_HPP

#include <filesystem>
#include <string>

namespace sniffercommit::domain::ports {
struct ExtractionResult {
  bool success_ = false;
  std::filesystem::path extracted_root_;
  std::string error_message_;
};

struct IArchiveExtractor {
  virtual ~IArchiveExtractor() = default;

  [[nodiscard]] virtual ExtractionResult extract(const std::filesystem::path& archive_path,
                                                 const std::filesystem::path& dest_dir) = 0;
};
}  // namespace sniffercommit::domain::ports

#endif  // !SNIFFERCOMMIT_DOMAIN_PORTS_ARCHIVE_EXTRACTOR_HPP
