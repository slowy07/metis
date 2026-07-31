#ifndef SNIFFERCOMMIT_DOMAIN_PORTS_HTTP_CLIENT_HPP
#define SNIFFERCOMMIT_DOMAIN_PORTS_HTTP_CLIENT_HPP

#include <filesystem>
#include <string>
namespace sniffercommit::domain::ports {

struct DownloadResult {
  bool success_ = false;
  std::filesystem::path download_path_;
  std::string error_message_;
};

struct IHttpClient {
  virtual ~IHttpClient() = default;

  [[nodiscard]] virtual DownloadResult download(const std::string& url,
                                                const std::filesystem::path& dest_dir,
                                                const std::string& filename = {}) = 0;
};
}  // namespace sniffercommit::domain::ports

#endif  // !SNIFFERCOMMIT_DOMAIN_PORTS_HTTP_CLIENT_HPP
