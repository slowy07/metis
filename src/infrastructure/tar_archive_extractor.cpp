#include "metis/infrastructure/tar_archive_extractor.hpp"

#include <fmt/format.h>

#include <string>

#include "metis/domain/ports/archive_extractor.hpp"
#include "metis/domain/ports/shell_executor.hpp"

namespace metis::infrastructure {

TarArchiveExtractor::TarArchiveExtractor(domain::ports::IShellExecutor* shell)
  : shell_(shell) {}

domain::ports::ExtractionResult TarArchiveExtractor::extract(
    const std::filesystem::path& archive_path, const std::filesystem::path& dest_dir) {
  domain::ports::ExtractionResult result;

  std::string flags = tar_flags_for_extension(archive_path);

  if (flags.empty()) {
    result.error_message_ =
        fmt::format("Unsupported archive format: {}", archive_path.extension().string());
    return result;
  }

  std::string cmd =
      fmt::format(R"(tar {} -C "{}" -xf "{}")", flags, dest_dir.string(), archive_path.string());

  auto exec_result = shell_->exec_captured(cmd);
  if (exec_result.exit_code_ != 0) {
    result.error_message_ = fmt::format("tar extraction failed (exit {}): {}",
                                        exec_result.exit_code_, exec_result.output_);
    return result;
  }

  result.success_ = true;
  result.extracted_root_ = dest_dir;

  return result;
}

std::string TarArchiveExtractor::tar_flags_for_extension(const std::filesystem::path& path) {
  std::string ext = path.string();

  if (ext.ends_with(".tar.gz") || ext.ends_with(".tgz")) {
    return "-z";
  }

  if (ext.ends_with(".tar.xz")) {
    return "-J";
  }

  if (ext.ends_with(".tar.bz2") || ext.ends_with(".tbz2")) {
    return "-j";
  }

  if (ext.ends_with(".tar")) {
    return "";
  }

  return {};
}

}  // namespace metis::infrastructure
