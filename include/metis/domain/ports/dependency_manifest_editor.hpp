#ifndef METIS_DOMAIN_PORTS_DEPENDENCY_MANIFEST_EDITOR_HPP
#define METIS_DOMAIN_PORTS_DEPENDENCY_MANIFEST_EDITOR_HPP

#include <filesystem>
#include <string>
#include <vector>

namespace metis::domain::ports {
struct ManifestEditResult {
  bool success = false;
  std::string message;
};

struct IDependencyManifestEditor {
  virtual ~IDependencyManifestEditor() = default;

  [[nodiscard]] virtual bool can_edit(const std::filesystem::path& repo_root) const = 0;

  [[nodiscard]] virtual ManifestEditResult add_dependency(const std::filesystem::path& repo_root,
                                                          const std::string& name,
                                                          const std::string& version) const = 0;

  [[nodiscard]] virtual ManifestEditResult remove_dependency(const std::filesystem::path& repo_root,
                                                             const std::string& name) const = 0;

  [[nodiscard]] virtual ManifestEditResult update_dependency(
      const std::filesystem::path& repo_root, const std::string& name,
      const std::string& new_version) const = 0;

  [[nodiscard]] virtual std::string source_name() const = 0;
};
}  // namespace metis::domain::ports

#endif  // !METIS_DOMAIN_PORTS_DEPENDENCY_MANIFEST_EDITOR_HPP
