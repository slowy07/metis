#ifndef METIS_APPLICATION_DEPENDENCY_COMPOSITE_HPP
#define METIS_APPLICATION_DEPENDENCY_COMPOSITE_HPP

#include <filesystem>
#include <memory>
#include <optional>
#include <vector>

#include "metis/domain/dependency.hpp"
#include "metis/domain/ports/dependency_manifest_editor.hpp"
#include "metis/domain/ports/dependency_parser.hpp"
#include "metis/domain/ports/dependency_version_checker.hpp"

namespace metis::application {
class DependencyParserComposite {
 public:
  void add_parser(std::unique_ptr<domain::ports::IDependencyParser> parser);

  [[nodiscard]] std::vector<domain::Dependency> parse_all(
      const std::filesystem::path& repo_root) const;

 private:
  std::vector<std::unique_ptr<domain::ports::IDependencyParser>> parsers_;
};

class DependencyVersionCheckerComposite {
 public:
  void add_checker(std::unique_ptr<domain::ports::IDependencyVersionChecker> checker);

  [[nodiscard]] std::optional<std::string> latest_version(const domain::Dependency& dep) const;

 private:
  std::vector<std::unique_ptr<domain::ports::IDependencyVersionChecker>> checkers_;
};

class DependencyManifestEditorComposite {
 public:
  void add_editor(std::unique_ptr<domain::ports::IDependencyManifestEditor> editor);

  [[nodiscard]] std::vector<domain::ports::IDependencyManifestEditor*> available_editors(
      const std::filesystem::path& repo_root) const;

  [[nodiscard]] domain::ports::IDependencyManifestEditor* find_editor(
      const std::string& source) const;

 private:
  std::vector<std::unique_ptr<domain::ports::IDependencyManifestEditor>> editors_;
};

}  // namespace metis::application

#endif  // !METIS_APPLICATION_DEPENDENCY_COMPOSITE_HPP
