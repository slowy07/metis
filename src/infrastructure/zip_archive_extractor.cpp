#include "metis/infrastructure/zip_archive_extractor.hpp"

#include <fmt/format.h>

#include "metis/domain/ports/archive_extractor.hpp"

namespace metis::infrastructure {
ZipArchiveExtractor::ZipArchiveExtractor(domain::ports::IShellExecutor* shell)
  : shell_(shell) {}

domain::ports::ExtractionResult ZipArchiveExtractor::extract(
    const std::filesystem::path& archive_path, const std::filesystem::path& dest_dir) {
  domain::ports::ExtractionResult result;

#ifdef _WIN32
  std::string cmd =
      fmt::format(R"(powershell -Command "Expand-Archive -Path '{}' -DestinationPath '{}' -Force")",
                  archive_path.string(), dest_dir.string());
#else
  std::string cmd =
      fmt::format(R"(unzip -o "{}" -d "{}")", archive_path.string(), dest_dir.string());
#endif  // _WIN32
  auto exec_result = shell_->exec_captured(cmd);

  if (exec_result.exit_code_ != 0) {
    result.error_message_ = fmt::format("zip extraction failed (exit {}): {}",
                                        exec_result.exit_code_, exec_result.output_);
    return result;
  }

  result.success_ = true;
  result.extracted_root_ = dest_dir;
  return result;
}
}  // namespace metis::infrastructure
