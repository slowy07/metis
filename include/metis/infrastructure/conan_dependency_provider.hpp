#ifndef METIS_INFRASTRUCTURE_CONAN_DEPENDENCY_PROVIDER_HPP
#define METIS_INFRASTRUCTURE_CONAN_DEPENDENCY_PROVIDER_HPP

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "metis/domain/dependency.hpp"
#include "metis/domain/ports/dependency_manifest_editor.hpp"
#include "metis/domain/ports/dependency_parser.hpp"
#include "metis/domain/ports/dependency_version_checker.hpp"
#include "metis/domain/ports/file_system.hpp"
#include "metis/domain/ports/shell_executor.hpp"

namespace metis::infrastructure {
class ConanDependencyParser : public domain::ports::IDependencyParser {
 public:
  explicit ConanDependencyParser(domain::ports::IFileSystem* fs);

  [[nodiscard]] bool can_parse(const std::filesystem::path& repo_root) const override;
  [[nodiscard]] std::vector<domain::Dependency> parse(
      const std::filesystem::path& repo_root) const override;
  [[nodiscard]] std::string source_name() const override;

 private:
  domain::ports::IFileSystem* fs_;
};

class ConanVersionChecker : public domain::ports::IDependencyVersionChecker {
 public:
  explicit ConanVersionChecker(domain::ports::IShellExecutor* shell);

  [[nodiscard]] std::optional<std::string> latest_version(
      const std::string& package_name) const override;
  [[nodiscard]] std::string source_name() const override;

 private:
  domain::ports::IShellExecutor* shell_;
};

class ConanManifestEditor : public domain::ports::IDependencyManifestEditor {
 public:
  explicit ConanManifestEditor(domain::ports::IFileSystem* fs);

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

#endif  // !METIS_INFRASTRUCTURE_CONAN_DEPENDENCY_PROVIDER_HPP
