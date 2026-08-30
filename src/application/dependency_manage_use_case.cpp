#include "metis/application/dependency_manage_use_case.hpp"

#include <fmt/format.h>

namespace metis::application {

DependencyManageUseCase::DependencyManageUseCase(
    std::unique_ptr<domain::ports::IFileSystem> file_system)
  : file_system_(std::move(file_system)) {}

void DependencyManageUseCase::register_editor(
    std::unique_ptr<domain::ports::IDependencyManifestEditor> editor) {
  editors_.add_editor(std::move(editor));
}

domain::DependencyManageResult DependencyManageUseCase::execute(
    const std::filesystem::path& repo_root, const DependencyManageOptions& opts) {
  domain::DependencyManageResult result;

  auto available = editors_.available_editors(repo_root);
  if (available.empty()) {
    result.success = false;
    result.add_message("No dependency manifest found (conanfile.py, vcpkg.json, CMakeLists.txt)");
    return result;
  }

  std::vector<domain::ports::IDependencyManifestEditor*> targets;

  if (!opts.source.empty()) {
    auto* editor = editors_.find_editor(opts.source);
    if (editor == nullptr || !editor->can_edit(repo_root)) {
      result.success = false;
      result.add_message(fmt::format("No manifest for source '{}' found", opts.source));
      return result;
    }
    targets.push_back(editor);
  } else {
    targets = available;
  }

  bool any_success = false;

  for (auto* editor : targets) {
    domain::ports::ManifestEditResult edit_result;

    switch (opts.action) {
      case DependencyManageOptions::Action::ADD:
        edit_result = editor->add_dependency(repo_root, opts.dependency_name, opts.version);
        break;
      case DependencyManageOptions::Action::REMOVE:
        edit_result = editor->remove_dependency(repo_root, opts.dependency_name);
        break;
      case DependencyManageOptions::Action::UPDATE:
        edit_result = editor->update_dependency(repo_root, opts.dependency_name, opts.version);
        break;
    }

    if (edit_result.success) {
      any_success = true;
      result.add_message(edit_result.message);
      result.add_modified(editor->source_name());
    } else {
      result.add_message(fmt::format("[{}] {}", editor->source_name(), edit_result.message));
    }
  }

  result.success = any_success;
  return result;
}

}  // namespace metis::application
