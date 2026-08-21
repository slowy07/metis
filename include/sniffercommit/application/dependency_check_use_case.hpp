#ifndef SNIFFERCOMMIT_APPLICATION_DEPENDENCY_CHECK_USE_CASE_HPP
#define SNIFFERCOMMIT_APPLICATION_DEPENDENCY_CHECK_USE_CASE_HPP

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "sniffercommit/domain/dependency.hpp"
#include "sniffercommit/domain/ports/file_system.hpp"
#include "sniffercommit/domain/ports/shell_executor.hpp"

namespace sniffercommit::application {

struct DependencyCheckOptions {
  bool verbose = false;
  bool generate_graph = false;
  std::string graph_output_path = "dependencies.dot";
};

class DependencyCheckUseCase {
 public:
  DependencyCheckUseCase(std::unique_ptr<domain::ports::IShellExecutor> shell,
                         std::unique_ptr<domain::ports::IFileSystem> file_system);

  [[nodiscard]] domain::DependencyCheckResult execute(const std::filesystem::path& repo_root,
                                                      const DependencyCheckOptions& opts);

 private:
  std::vector<domain::Dependency> parse_conanfile(const std::filesystem::path& repo_root);
  std::vector<domain::Dependency> parse_vcpkg_json(const std::filesystem::path& repo_root);
  std::vector<domain::Dependency> parse_cmake_fetchcontent(const std::filesystem::path& repo_root);

  static bool is_valid_semver(std::string_view version);
  [[nodiscard]] bool conan_dep_installed(const std::string& name) const;
  [[nodiscard]] bool vcpkg_dep_installed(const std::string& name) const;

  void check_lockfiles(const std::filesystem::path& repo_root,
                       domain::DependencyCheckResult& out) const;
  static void detect_duplicates(const std::vector<domain::Dependency>& all,
                         domain::DependencyCheckResult& out);
  static void generate_dot_graph(const std::vector<domain::Dependency>& all,
                          const std::filesystem::path& out_path);

  std::unique_ptr<domain::ports::IShellExecutor> shell_;
  std::unique_ptr<domain::ports::IFileSystem> file_system_;
};

}  // namespace sniffercommit::application

#endif  // !SNIFFERCOMMIT_APPLICATION_DEPENDENCY_CHECK_USE_CASE_HPP
