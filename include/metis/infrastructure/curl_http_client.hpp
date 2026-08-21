#ifndef METIS_INFRASTRUCTURE_CURL_HTTP_CLIENT_HPP
#define METIS_INFRASTRUCTURE_CURL_HTTP_CLIENT_HPP

#include <filesystem>
#include <string>

#include "metis/domain/ports/http_client.hpp"
#include "metis/domain/ports/shell_executor.hpp"

namespace metis::infrastructure {
class CurlHttpClient : public domain::ports::IHttpClient {
 public:
  explicit CurlHttpClient(domain::ports::IShellExecutor* shell);

  [[nodiscard]] domain::ports::DownloadResult download(const std::string& url,
                                                       const std::filesystem::path& dest_dir,
                                                       const std::string& filename = {}) override;

 private:
  domain::ports::IShellExecutor* shell_;

  [[nodiscard]] bool has_curl() const;
  [[nodiscard]] bool has_wget() const;
  [[nodiscard]] std::string basename_for_url(const std::string& url) const;
};
}  // namespace metis::infrastructure

#endif  // !METIS_INFRASTRUCTURE_CURL_HTTP_CLIENT_HPP
