#ifndef METIS_INFRASTRUCTURE_CMAKE_DEPENDENCY_PROVIDER_HPP
#define METIS_INFRASTRUCTURE_CMAKE_DEPENDENCY_PROVIDER_HPP

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "metis/domain/dependency.hpp"
#include "metis/domain/ports/dependency_manifest_editor.hpp"
#include "metis/domain/ports/dependency_parser.hpp"
#include "metis/domain/ports/dependency_version_checker.hpp"
#include "metis/domain/ports/file_system.hpp"
#include "metis/domain/ports/http_client.hpp"

namespace metis::infrastructure {
class CMakeDependencyParser : public domain::ports::IDependencyParser {
 public:
  explicit CMakeDependencyParser(domain::ports::IFileSystem* fs);

  [[nodiscard]] bool can_parse(const std::filesystem::path& repo_root) const override;
  [[nodiscard]] std::vector<domain::Dependency> parse(
      const std::filesystem::path& repo_root) const override;
  [[nodiscard]] std::string source_name() const override;

 private:
  domain::ports::IFileSystem* fs_;
};

class CMakeVersionChecker : public domain::ports::IDependencyVersionChecker {
 public:
  explicit CMakeVersionChecker(domain::ports::IHttpClient* http);

  [[nodiscard]] std::optional<std::string> latest_version(
      const std::string& package_name) const override;
  [[nodiscard]] std::string source_name() const override;

 private:
  domain::ports::IHttpClient* http_;
};

class CMakeManifestEditor : public domain::ports::IDependencyManifestEditor {
 public:
  explicit CMakeManifestEditor(domain::ports::IFileSystem* fs);

  [[nodiscard]] bool can_edit(const std::filesystem::path& repo_root) const override;
  [[nodiscard]] domain::ports::ManifestEditResult add_dependency(
      const std::filesystem::path& repo_root, const std::string& name,
      const std::string& version) const override;
  [[nodiscard]] domain::ports::ManifestEditResult remove_dependency(
      const std::filesystem::path& repo_root, const std::string& name) const override;
  [[nodiscard]] domain::ports::ManifestEditResult update_dependency(
      const std::filesystem::path& repo_root, const std::string& name,
      const std::string& new_version) const override;
  [[nodiscard]] std::string source_name() const override;

 private:
  domain::ports::IFileSystem* fs_;
};

}  // namespace metis::infrastructure

#endif  // ! METIS_INFRASTRUCTURE_CMAKE_DEPENDENCY_PROVIDER_HPP
