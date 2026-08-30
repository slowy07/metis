#ifndef METIS_APPLICATION_DEPENDENCY_MANAGE_USE_CASE_HPP
#define METIS_APPLICATION_DEPENDENCY_MANAGE_USE_CASE_HPP

#include <memory>
#include <string>
#include <vector>

#include "metis/application/dependency_composite.hpp"
#include "metis/domain/dependency.hpp"
#include "metis/domain/ports/file_system.hpp"

namespace metis::application {

struct DependencyManageOptions {
  enum class Action { ADD, REMOVE, UPDATE };

  Action action = Action::ADD;
  std::string dependency_name;
  std::string version;  // For add/update
  std::string source;   // Optional: restrict to specific source (conan, vcpkg, cmake)
  bool yes = false;     // Non-interactive mode
};

class DependencyManageUseCase {
 public:
  explicit DependencyManageUseCase(std::unique_ptr<domain::ports::IFileSystem> file_system);

  void register_editor(std::unique_ptr<domain::ports::IDependencyManifestEditor> editor);

  [[nodiscard]] domain::DependencyManageResult execute(const std::filesystem::path& repo_root,
                                                       const DependencyManageOptions& opts);

 private:
  std::unique_ptr<domain::ports::IFileSystem> file_system_;
  DependencyManifestEditorComposite editors_;
};

}  // namespace metis::application

#endif  // !METIS_APPLICATION_DEPENDENCY_MANAGE_USE_CASE_HPP
