#include "metis/application/dependency_composite.hpp"

#include <utility>
#include <vector>

#include "metis/domain/dependency.hpp"

namespace metis::application {
void DependencyParserComposite::add_parser(
    std::unique_ptr<domain::ports::IDependencyParser> parser) {
  parsers_.push_back(std::move(parser));
}

std::vector<domain::Dependency> DependencyParserComposite::parse_all(
    const std::filesystem::path& repo_root) const {
  std::vector<domain::Dependency> all;

  for (const auto& parser : parsers_) {
    if (parser->can_parse(repo_root)) {
      auto deps = parser->parse(repo_root);
      all.insert(all.end(), deps.begin(), deps.end());
    }
  }

  return all;
}

void DependencyVersionCheckerComposite::add_checker(
    std::unique_ptr<domain::ports::IDependencyVersionChecker> checker) {
  checkers_.push_back(std::move(checker));
}

std::optional<std::string> DependencyVersionCheckerComposite::latest_version(
    const domain::Dependency& dep) const {
  for (const auto& checker : checkers_) {
    if (checker->source_name() == dep.source) {
      return checker->latest_version(dep.name);
    }
  }
  return std::nullopt;
}

void DependencyManifestEditorComposite::add_editor(
    std::unique_ptr<domain::ports::IDependencyManifestEditor> editor) {
  editors_.push_back(std::move(editor));
}

std::vector<domain::ports::IDependencyManifestEditor*>
DependencyManifestEditorComposite::available_editors(const std::filesystem::path& repo_root) const {
  std::vector<domain::ports::IDependencyManifestEditor*> out;
  for (const auto& editor : editors_) {
    if (editor->can_edit(repo_root)) {
      out.push_back(editor.get());
    }
  }
  return out;
}

domain::ports::IDependencyManifestEditor* DependencyManifestEditorComposite::find_editor(
    const std::string& source) const {
  for (const auto& editor : editors_) {
    if (editor->source_name() == source ||
        (source == "cmake" && editor->source_name() == "cmake-fetchcontent")) {
      return editor.get();
    }
  }
  return nullptr;
}

}  // namespace metis::application
