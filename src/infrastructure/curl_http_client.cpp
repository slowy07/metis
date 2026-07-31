#include "sniffercommit/infrastructure/curl_http_client.hpp"

#include <fmt/format.h>

#include <string>

#include "sniffercommit/domain/ports/http_client.hpp"
#include "sniffercommit/domain/ports/shell_executor.hpp"

namespace sniffercommit::infrastructure {
CurlHttpClient::CurlHttpClient(domain::ports::IShellExecutor* shell) : shell_(shell) {}

domain::ports::DownloadResult CurlHttpClient::download(const std::string& url,
                                                       const std::filesystem::path& dest_dir,
                                                       const std::string& filename) {
  domain::ports::DownloadResult result;

  std::string target_name = filename.empty() ? basename_for_url(url) : filename;
  if (target_name.empty()) {
    result.error_message_ = "Could not determine filename from URL and no filename provided";
    return result;
  }

  auto dest_path = dest_dir / target_name;

  if (has_curl()) {
    std::string cmd = fmt::format(R"(curl -Lf -o "{}" "{}")", dest_path.string(), url);
    auto exec_result = shell_->exec_captured(cmd);
    if (exec_result.exit_code_ != 0) {
      result.error_message_ =
          fmt::format("curl failed (exit {}): {}", exec_result.exit_code_, exec_result.output_);
      return result;
    }
  } else if (has_wget()) {
    std::string cmd = fmt::format(R"(wget -O "{}" "{}")", dest_path.string(), url);
    auto exec_result = shell_->exec_captured(cmd);
    if (exec_result.exit_code_ != 0) {
      result.error_message_ =
          fmt::format("wget failed (exit {}): {}", exec_result.exit_code_, exec_result.output_);
      return result;
    }
  } else {
    result.error_message_ = "Neither curl nor wget is available. please install one of them";
    return result;
  }

  result.success_ = true;
  result.download_path_ = dest_path;
  return result;
}

bool CurlHttpClient::has_curl() const { return shell_->command_exists("curl"); }

bool CurlHttpClient::has_wget() const { return shell_->command_exists("wget"); }

std::string CurlHttpClient::basename_for_url(const std::string& url) const {
  size_t last_slash = url.find_last_of('/');
  if (last_slash != std::string::npos && last_slash + 1 < url.size()) {
    return url.substr(last_slash + 1);
  }

  return {};
}
}  // namespace sniffercommit::infrastructure
