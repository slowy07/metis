#ifndef METIS_INFRASTRUCTURE_CHECK_CACHE_HPP
#define METIS_INFRASTRUCTURE_CHECK_CACHE_HPP

#include <filesystem>
#include <mutex>
#include <string>
#include <vector>

namespace metis::infrastructure {
class CheckCache {
 public:
  explicit CheckCache(std::filesystem::path repo_root);

  [[nodiscard]] bool lookup(const std::string& check_name, const std::string& check_command,
                            const std::vector<std::string>& check_args,
                            const std::vector<std::string>& files, int& out_exit_code) const;

  void store(const std::string& check_name, const std::string& check_command,
             const std::vector<std::string>& check_args, const std::vector<std::string>& files,
             int exit_code);

  void clear();

 private:
  [[nodiscard]] std::string make_key(const std::string& check_name,
                                     const std::string& check_command,
                                     const std::vector<std::string>& check_args,
                                     const std::vector<std::string>& files) const;

  struct Entry {
    std::string key;
    int exit_code;
  };

  [[nodiscard]] std::vector<Entry> load() const;
  void save(const std::vector<Entry>& entries) const;

  std::filesystem::path cache_dir_;
  std::filesystem::path cache_file_;
  mutable std::mutex mutex_;
};
}  // namespace metis::infrastructure

#endif  // !METIS_INFRASTRUCTURE_CHECK_CACHE_HPP
